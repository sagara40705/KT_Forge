#include <Renderer/Effects/EffectPlayer.h>
#include <Renderer/RenderData.h>
#include <Graphics/Commands/CommandContext.h>
#include <Graphics/Commands/CommandQueue.h>
#include <Graphics/Textures/ColorTargetView.h>
#include <Graphics/Textures/DepthTargetView.h>
#include <Graphics/GraphicsValidation.h>
#include <Effekseer.h>
#include <EffekseerRendererDX12.h>
#include <Effekseer/Effekseer.EffectNode.h>
#include <EffekseerRendererCommon/TextureLoader.h>
#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace KT::Renderer
{
	bool IsValidEffectTransform(const KT::Core::Math::Matrix4& matrix) noexcept
	{
		if (!KT::Core::Math::IsAffine(matrix))
		{
			return false;
		}

		// SetMatrix内部のSRT分解は、各基底の長さの二乗で割る。
		for (std::size_t row = 0; row < 3; ++row)
		{
			const float squaredLength = matrix(row, 0) * matrix(row, 0) + matrix(row, 1) * matrix(row, 1) + matrix(row, 2) * matrix(row, 2);
			if (!std::isfinite(squaredLength) || squaredLength <= 0)
			{
				return false;
			}
			if (!std::isfinite(std::sqrt(squaredLength) / squaredLength))
			{
				return false;
			}
		}
		return true;
	}

	namespace
	{
		// 同じPlayer内だけで有効な値handleを発行する。
		std::atomic<std::uint64_t> nextOwner{1};

		// GPU Particleの宣言だけを検出し、資源を作らず読込完了後に拒否する。
		class DetectGpuParticle : public Effekseer::GpuParticleFactory
		{
		public:
			explicit DetectGpuParticle(bool& detected)
				: detected_(detected)
			{
			}

			Effekseer::GpuParticles::ResourceRef CreateResource(const Effekseer::GpuParticles::ParamSet&, const Effekseer::Effect*) override
			{
				detected_ = true;
				return nullptr;
			}

		private:
			bool& detected_;
		};

		std::uint64_t AllocateOwner()
		{
			auto id = nextOwner.load();
			for (;;)
			{
				if (id == (std::numeric_limits<std::uint64_t>::max)())
				{
					throw std::overflow_error("EffectPlayerの所有IDが上限に達しました。");
				}
				if (nextOwner.compare_exchange_weak(id, id + 1))
				{
					return id;
				}
			}
		}

		Effekseer::Matrix44 CameraMatrix(const KT::Core::Math::Matrix4& source)
		{
			Effekseer::Matrix44 matrix;
			for (std::size_t row = 0; row < 4; ++row)
			{
				for (std::size_t column = 0; column < 4; ++column)
				{
					matrix.Values[row][column] = source(row, column);
				}
			}
			return matrix;
		}

		Effekseer::Matrix43 WorldMatrix(const KT::Core::Math::Matrix4& source)
		{
			Effekseer::Matrix43 matrix;
			for (std::size_t row = 0; row < 4; ++row)
			{
				for (std::size_t column = 0; column < 3; ++column)
				{
					matrix.Value[row][column] = source(row, column);
				}
			}
			return matrix;
		}

		// 初版の対応範囲を読込時に検査し、深度を変更しない設定へ統一する。
		void PrepareNodes(Effekseer::EffectNode* root)
		{
			std::vector<Effekseer::EffectNode*> nodes{root};
			for (std::size_t index = 0; index < nodes.size(); ++index)
			{
				auto* node = nodes[index];
				auto* implementation = dynamic_cast<Effekseer::EffectNodeImplemented*>(node);
				if (!implementation || implementation->GpuParticlesResource != nullptr)
				{
					throw std::invalid_argument("初版のEffectPlayerはGPU Particleを扱いません。");
				}
				if (node->GetType() != Effekseer::EffectNodeType::Root && node->GetType() != Effekseer::EffectNodeType::NoneType)
				{
					auto parameters = node->GetBasicRenderParameter();
					if (parameters.MaterialType != Effekseer::RendererMaterialType::Default || parameters.Distortion ||
						(parameters.AlphaBlend != Effekseer::AlphaBlendType::Blend &&
							parameters.AlphaBlend != Effekseer::AlphaBlendType::Add))
					{
						throw std::invalid_argument(
							"初版のEffectPlayerは通常Materialの半透明・加算だけに対応します。歪み・独自Materialは未対応です。");
					}
					parameters.ZWrite = false;
					parameters.ZTest = true;
					parameters.SoftParticleDistanceFar = 0;
					parameters.SoftParticleDistanceNear = 0;
					parameters.SoftParticleDistanceNearOffset = 0;
					node->SetBasicRenderParameter(parameters);
					// 固定版のSetterはSoftParticle値を反映しないため、読込候補の内部値だけを無効化する。
					implementation->RendererCommon.BasicParameter.SoftParticleDistanceFar = 0;
					implementation->RendererCommon.BasicParameter.SoftParticleDistanceNear = 0;
					implementation->RendererCommon.BasicParameter.SoftParticleDistanceNearOffset = 0;
				}
				for (int child = 0; child < node->GetChildrenCount(); ++child)
				{
					nodes.push_back(node->GetChild(child));
				}
			}
		}
	}

	struct EffectPlayer::Impl
	{
		// 同期1slotの利用完了は、同じDirect QueueのFenceで確認する。
		KT::Graphics::CommandQueue& queue;
		ID3D12Device* device;
		DXGI_FORMAT targetFormat;
		std::uint64_t owner = AllocateOwner();
		double seconds = 0;
		int drawCalls = 0;
		bool gpuParticleDetected = false;
		bool framePrepared = false;
		// 素材はPlayerの終了まで共有する。再生終了後もGPU完了前に解放しない。
		std::unordered_map<std::uint64_t, Effekseer::EffectRef> assets;
		std::unordered_map<std::uint64_t, Effekseer::Handle> playing;
		std::unordered_map<std::uint64_t, EffectAsset> instanceAssets;
		EffekseerRenderer::RendererRef renderer;
		Effekseer::ManagerRef manager;
		Effekseer::RefPtr<EffekseerRenderer::SingleFrameMemoryPool> memory;
		Effekseer::RefPtr<EffekseerRenderer::CommandList> commands;

		Impl(KT::Graphics::GraphicsDevice& graphics, KT::Graphics::CommandQueue& queueValue, DXGI_FORMAT format)
			: queue(queueValue),
			  device(graphics.GetDevice()),
			  targetFormat(format)
		{
			KT::Graphics::RequireSameDevice(queue.GetCommandQueue(), device);
			if (!KT::Graphics::IsColorTargetFormat(format) || owner == 0)
			{
				throw std::invalid_argument("EffectPlayerの描画formatまたは所有IDが無効です。");
			}
			// 一つの外部Listに記録し、LH・D32・Reverse-Zで既存描画へ合わせる。
			renderer = EffekseerRendererDX12::Create(device, queue.GetCommandQueue(), 1, &format, 1, DXGI_FORMAT_D32_FLOAT, true, 8192);
			manager = Effekseer::Manager::Create(8192);
			if (!renderer || !manager)
			{
				throw std::runtime_error("EffekseerのDX12 RendererまたはManagerを作成できませんでした。");
			}
			manager->SetCoordinateSystem(Effekseer::CoordinateSystem::LH);
			manager->GetSetting()->SetGpuParticleFactory(Effekseer::MakeRefPtr<DetectGpuParticle>(gpuParticleDetected));
			manager->SetSpriteRenderer(renderer->CreateSpriteRenderer());
			manager->SetRibbonRenderer(renderer->CreateRibbonRenderer());
			manager->SetRingRenderer(renderer->CreateRingRenderer());
			manager->SetTrackRenderer(renderer->CreateTrackRenderer());
			manager->SetModelRenderer(renderer->CreateModelRenderer());
			manager->SetTextureLoader(EffekseerRenderer::CreateTextureLoader(renderer->GetGraphicsDevice(), nullptr,
				format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ? Effekseer::ColorSpaceType::Linear : Effekseer::ColorSpaceType::Gamma));
			manager->SetModelLoader(renderer->CreateModelLoader());
			manager->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>());
			// SRGBのRTVへ線形色を書き、上流のガンマ維持変換を有効にする。
			renderer->SetMaintainGammaColorInLinearColorSpace(format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
			memory = EffekseerRenderer::CreateSingleFrameMemoryPool(renderer->GetGraphicsDevice());
			commands = EffekseerRenderer::CreateCommandList(renderer->GetGraphicsDevice(), memory);
			if (!memory || !commands)
			{
				throw std::runtime_error("EffekseerのFrame記録資源を作成できませんでした。");
			}
		}

		void Wait()
		{
			// 外部Frameの送信も含めて完了させ、再生停止・読込・pool再利用を安全にする。
			queue.Wait(queue.Signal());
		}
	};

	EffectPlayer::EffectPlayer(KT::Graphics::GraphicsDevice& device, KT::Graphics::CommandQueue& queue, DXGI_FORMAT targetFormat)
		: impl_(std::make_unique<Impl>(device, queue, targetFormat))
	{
	}

	EffectPlayer::~EffectPlayer()
	{
		// GPUが参照する上流の素材・heap・VBを、完了後に破棄する。
		impl_->Wait();
	}

	EffectAsset EffectPlayer::Load(const std::filesystem::path& path)
	{
		if (path.extension() != L".efkefc" || !std::filesystem::is_regular_file(path))
		{
			throw std::invalid_argument("Effect素材には存在する.efkefcファイルを指定してください。");
		}
		if (impl_->assets.size() >= 128)
		{
			throw std::overflow_error("EffectPlayerの素材数が初版の上限128に達しました。");
		}
		impl_->Wait();
		const auto absolute = std::filesystem::absolute(path).u16string();
		if (absolute.size() >= 512)
		{
			throw std::invalid_argument("Effect素材の絶対パスはEffekseerの対応範囲512文字未満にしてください。");
		}
		impl_->gpuParticleDetected = false;
		auto effect = Effekseer::Effect::Create(impl_->manager, absolute.c_str());
		if (!effect || !effect->GetRoot())
		{
			throw std::runtime_error("Effekseerの素材読込に失敗しました: " + path.string());
		}
		if (impl_->gpuParticleDetected)
		{
			throw std::invalid_argument("初版のEffectPlayerはGPU Particleを扱いません。");
		}
		if (effect->GetWaveCount() != 0 || effect->GetMaterialCount() != 0)
		{
			throw std::invalid_argument("初版のEffectPlayerは素材内の音声・独自Materialに対応していません。");
		}
		// 上流Loaderが欠落Textureを許容しても、制作時には欠落として診断する。
		for (int index = 0; index < effect->GetColorImageCount(); ++index)
		{
			if (!effect->GetColorImage(index))
			{
				throw std::runtime_error("Effect素材が参照する色Textureを読み込めませんでした。");
			}
		}
		for (int index = 0; index < effect->GetModelCount(); ++index)
		{
			if (!effect->GetModel(index))
			{
				throw std::runtime_error("Effect素材が参照するModelを読み込めませんでした。");
			}
		}
		for (int index = 0; index < effect->GetCurveCount(); ++index)
		{
			if (!effect->GetCurve(index))
			{
				throw std::runtime_error("Effect素材が参照するCurveを読み込めませんでした。");
			}
		}
		PrepareNodes(effect->GetRoot());
		const auto id = static_cast<std::uint64_t>(impl_->assets.size()) + 1;
		impl_->assets.emplace(id, std::move(effect));
		return {impl_->owner, id};
	}

	bool EffectPlayer::Contains(EffectAsset asset) const noexcept
	{
		return asset.owner == impl_->owner && impl_->assets.contains(asset.id);
	}

	void EffectPlayer::Update(std::span<const EffectInstance> instances, double deltaSeconds)
	{
		if (!std::isfinite(deltaSeconds) || deltaSeconds < 0 || deltaSeconds > 1 || instances.size() > 256)
		{
			throw std::invalid_argument("Effect更新はdelta 0..1秒、同時再生256個体までに対応します。");
		}
		// 個体の全入力を検査してから、上流Managerの再生状態を変更する。
		std::unordered_map<std::uint64_t, EffectAsset> desired;
		for (const auto& instance : instances)
		{
			if (instance.id == 0 || instance.asset.owner != impl_->owner || !impl_->assets.contains(instance.asset.id) ||
				!IsValidEffectTransform(instance.world) || !desired.emplace(instance.id, instance.asset).second)
			{
				throw std::invalid_argument("Effect個体のID・素材・World行列または重複指定が不正です。");
			}
			if (const auto found = impl_->instanceAssets.find(instance.id);
				found != impl_->instanceAssets.end() && found->second != instance.asset)
			{
				throw std::invalid_argument("再生中のEffect handleの素材を変更できません。");
			}
		}
		impl_->Wait();
		try
		{
			for (auto iterator = impl_->playing.begin(); iterator != impl_->playing.end();)
			{
				if (!desired.contains(iterator->first))
				{
					impl_->manager->StopEffect(iterator->second);
					iterator = impl_->playing.erase(iterator);
				}
				else
				{
					++iterator;
				}
			}
			for (const auto& instance : instances)
			{
				auto found = impl_->playing.find(instance.id);
				if (found == impl_->playing.end())
				{
					const auto handle = impl_->manager->Play(impl_->assets.at(instance.asset.id), 0, 0, 0);
					if (handle < 0)
					{
						throw std::runtime_error("Effekseerの再生個体を生成できませんでした。Particle容量を確認してください。");
					}
					found = impl_->playing.emplace(instance.id, handle).first;
				}
				impl_->manager->SetMatrix(found->second, WorldMatrix(instance.world));
			}
			impl_->instanceAssets.swap(desired);
			impl_->manager->Update(static_cast<float>(deltaSeconds * 60));
			impl_->seconds += deltaSeconds;
		}
		catch (...)
		{
			// 上流再生の失敗は部分継続せず、全個体を停止して呼出側へ返す。
			StopAll();
			throw;
		}
	}

	void EffectPlayer::StopAll()
	{
		impl_->Wait();
		impl_->manager->StopAllEffects();
		impl_->manager->Update(0);
		impl_->playing.clear();
		impl_->instanceAssets.clear();
	}

	bool EffectPlayer::IsPlaying(std::uint64_t id) const
	{
		const auto found = impl_->playing.find(id);
		return found != impl_->playing.end() && impl_->manager->Exists(found->second);
	}

	std::uint64_t EffectPlayer::OwnerId() const noexcept
	{
		return impl_->owner;
	}

	std::size_t EffectPlayer::PlayingCount() const
	{
		std::size_t count = 0;
		for (const auto& [id, handle] : impl_->playing)
		{
			(void)id;
			count += impl_->manager->Exists(handle) ? 1 : 0;
		}
		return count;
	}

	int EffectPlayer::LastDrawCalls() const noexcept
	{
		return impl_->drawCalls;
	}

	void EffectPlayer::PrepareFrame(KT::Graphics::CommandQueue& queue)
	{
		if (queue.GetCommandQueue() != impl_->queue.GetCommandQueue())
		{
			throw std::invalid_argument("EffectPlayerとFrameのDirect Queueは同一である必要があります。");
		}
		impl_->Wait();
		impl_->memory->NewFrame();
		impl_->framePrepared = true;
		impl_->drawCalls = 0;
	}

	void EffectPlayer::Record(KT::Graphics::CommandContext& commands, const KT::Graphics::ColorTargetView& color,
		const KT::Graphics::DepthTargetView& depth, const RenderView& view)
	{
		bool begun = false;
		try
		{
			if (!impl_->framePrepared)
			{
				throw std::logic_error("Effect描画はGraph記録前にPrepareFrameを完了してください。");
			}
			impl_->framePrepared = false;
			auto* list = commands.GetRecordingList();
			KT::Graphics::RequireSameDevice(list, impl_->device);
			KT::Graphics::RequireSameDevice(color.GetResource(), impl_->device);
			KT::Graphics::RequireSameDevice(depth.GetResource(), impl_->device);
			if (!depth.IsReadOnly() || color.GetFormat() != impl_->targetFormat || view.width != color.GetWidth() ||
				view.height != color.GetHeight() || depth.GetWidth() != view.width || depth.GetHeight() != view.height ||
				!KT::Core::Math::IsFinite(view.view) || !KT::Core::Math::IsFinite(view.projection))
			{
				throw std::invalid_argument("Effect描画のreadonly深度・format・Camera・寸法が一致しません。");
			}
			EffekseerRendererDX12::BeginCommandList(impl_->commands, list);
			begun = true;
			impl_->renderer->SetCommandList(impl_->commands);
			impl_->renderer->ResetDrawCallCount();
			impl_->renderer->SetCameraMatrix(CameraMatrix(view.view));
			impl_->renderer->SetProjectionMatrix(CameraMatrix(view.projection));
			impl_->renderer->SetTime(static_cast<float>(std::fmod(impl_->seconds, 3600.0)));
			Effekseer::Manager::LayerParameter layer;
			layer.ViewerPosition = {view.position.x, view.position.y, view.position.z};
			impl_->manager->SetLayerParameter(0, layer);
			// 出力とviewportはGraphの借用viewから設定する。Effekseerへ画像の所有を渡さない。
			const auto rtv = color.GetRtv();
			const auto dsv = depth.GetDsv();
			list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
			const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(view.width), static_cast<float>(view.height), 0, 1};
			const D3D12_RECT scissor{0, 0, static_cast<LONG>(view.width), static_cast<LONG>(view.height)};
			list->RSSetViewports(1, &viewport);
			list->RSSetScissorRects(1, &scissor);
			if (!impl_->renderer->BeginRendering())
			{
				throw std::runtime_error("Effekseerの描画開始に失敗しました。");
			}
			Effekseer::Manager::DrawParameter parameters;
			parameters.ZNear = 1;
			parameters.ZFar = 0;
			parameters.ViewProjectionMatrix = impl_->renderer->GetCameraProjectionMatrix();
			impl_->manager->Draw(parameters);
			if (!impl_->renderer->EndRendering())
			{
				throw std::runtime_error("Effekseerの描画終了に失敗しました。");
			}
			impl_->drawCalls = impl_->renderer->GetDrawCallCount();
			impl_->renderer->SetCommandList(nullptr);
			EffekseerRendererDX12::EndCommandList(impl_->commands);
		}
		catch (...)
		{
			impl_->renderer->SetCommandList(nullptr);
			if (begun)
			{
				EffekseerRendererDX12::EndCommandList(impl_->commands);
			}
			commands.Invalidate();
			throw;
		}
	}
}
