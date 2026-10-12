#include <Renderer/StaticSceneRenderer.h>
#include <Renderer/PreparedRenderFrame.h>
#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Passes/ClearPass.h>
#include <Renderer/Passes/DepthClearPass.h>
#include <Renderer/Passes/OpaquePass.h>
#include <Renderer/Passes/ShadowPass.h>
#include <Renderer/Passes/EffectPass.h>
#include <Renderer/Effects/EffectPlayer.h>
#include <Renderer/Passes/UiPass.h>
#include <Renderer/RenderConstants.h>
#include <Graphics/Textures/DepthTargetView.h>
#include <Graphics/GraphicsValidation.h>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace KT::Renderer
{
	StaticSceneRenderer::StaticSceneRenderer(KT::Graphics::GraphicsDevice& device, KT::Graphics::CommandQueue& queue,
		const KT::Content::AssetCatalog& assets, DXGI_FORMAT targetFormat)
		: device_(device),
		  queue_(queue),
		  assets_(assets),
		  targetFormat_(targetFormat),
		  meshes_(device),
		  materials_(device, targetFormat)
	{
		if (!assets.IsFrozen())
		{
			throw std::invalid_argument("StaticSceneRendererには固定済みAssetCatalogが必要です。");
		}
		KT::Graphics::RequireSameDevice(queue.GetCommandQueue(), device.GetDevice());
	}

	std::uint64_t StaticSceneRenderer::ResolveMesh(KT::Content::MeshAssetId id)
	{
		if (const auto found = meshIds_.find(id.Value()); found != meshIds_.end())
		{
			return found->second;
		}
		const auto source = assets_.Resolve(id);
		if (source.state != KT::Content::AssetReferenceState::Ready)
		{
			throw std::invalid_argument("GPU化するMesh素材が欠落または種類違いです: " + id.ToString());
		}
		if (meshIds_.size() == (std::numeric_limits<std::uint64_t>::max)())
		{
			throw std::overflow_error("GPU Meshのhandleを発行できません。");
		}
		const auto handle = static_cast<std::uint64_t>(meshIds_.size()) + 1;
		// 対応表のnodeを先に確保し、Storeへの所有移転後は確保せず公開する。
		const auto [entry, inserted] = meshIds_.emplace(id.Value(), 0);
		(void)inserted;
		try
		{
			auto mesh = std::make_unique<Mesh>(device_, *source.asset);
			mesh->Upload(queue_);
			meshes_.Add(handle, std::move(mesh));
			entry->second = handle;
		}
		catch (...)
		{
			meshIds_.erase(entry);
			throw;
		}
		return handle;
	}

	std::uint64_t StaticSceneRenderer::ResolveMaterial(KT::Content::MaterialAssetId id)
	{
		if (const auto found = materialIds_.find(id.Value()); found != materialIds_.end())
		{
			return found->second;
		}
		const auto source = assets_.Resolve(id);
		if (source.state != KT::Content::AssetReferenceState::Ready)
		{
			throw std::invalid_argument("GPU化するMaterial素材が欠落または種類違いです: " + id.ToString());
		}
		if (materialIds_.size() == (std::numeric_limits<std::uint64_t>::max)())
		{
			throw std::overflow_error("GPU Materialのhandleを発行できません。");
		}
		const auto handle = static_cast<std::uint64_t>(materialIds_.size()) + 1;
		const auto [entry, inserted] = materialIds_.emplace(id.Value(), 0);
		(void)inserted;
		try
		{
			const auto texture = source.asset->Settings().baseColorTexture;
			materials_.AddSurface(handle, *source.asset, texture ? &ResolveTexture(*texture) : nullptr);
			entry->second = handle;
		}
		catch (...)
		{
			materialIds_.erase(entry);
			throw;
		}
		return handle;
	}

	const KT::Graphics::SampledTexture& StaticSceneRenderer::ResolveTexture(KT::Content::TextureAssetId id)
	{
		if (const auto found = textures_.find(id.Value()); found != textures_.end())
		{
			return *found->second;
		}
		const auto source = assets_.Resolve(id);
		if (!source.asset)
		{
			throw std::invalid_argument("GPU化するTexture素材が欠落または種類違いです: " + id.ToString());
		}
		source.asset->ValidateLoaded();
		std::vector<KT::Graphics::TextureMipData> mips;
		for (const auto& mip : source.asset->Mips())
		{
			mips.push_back({mip.width, mip.height, mip.pixels});
		}
		const auto format = source.asset->ColorSpace() == KT::Content::TextureColorSpace::Srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB :
																								 DXGI_FORMAT_R8G8B8A8_UNORM;
		auto texture = std::make_unique<KT::Graphics::SampledTexture>(device_, mips, format);
		texture->Upload(queue_);
		const auto [found, inserted] = textures_.emplace(id.Value(), std::move(texture));
		(void)inserted;
		return *found->second;
	}

	void StaticSceneRenderer::Submit(const RenderCandidates& candidates, const std::optional<RenderView>& view,
		const KT::Graphics::ColorTargetView& color, D3D12_RESOURCE_STATES colorState, const SceneLighting& lighting, EffectPlayer* effects,
		UiRenderer* ui)
	{
		Wait();
		lastDrawCount_ = 0;
		if (color.GetFormat() != targetFormat_)
		{
			throw std::invalid_argument("静的Sceneの描画先formatがRenderer生成時の指定と一致しません。");
		}
		KT::Graphics::RequireSameDevice(color.GetResource(), device_.GetDevice());
		if (view && (view->width != color.GetWidth() || view->height != color.GetHeight()))
		{
			throw std::invalid_argument("Cameraと描画先の寸法が一致していません。");
		}
		if (submittedFrames_ == (std::numeric_limits<std::uint64_t>::max)())
		{
			throw std::overflow_error("描画の送信番号が上限に達しました。");
		}

		lighting.Validate();
		RenderWorld world;
		if (view)
		{
			world.objects.reserve(candidates.objects.size());
			for (const auto& object : candidates.objects)
			{
				world.objects.push_back({ResolveMesh(object.mesh), ResolveMaterial(object.material), object.world, object.submesh, object.skinPalette});
			}
		}
		// Viewと各Drawの定数にSkin palette容量を加え、加算前にUINT上限を確認する。
		constexpr std::size_t maxCapacity = (std::numeric_limits<UINT>::max)() & ~std::size_t{255};
		constexpr std::size_t bytesPerDraw = 512 + KT::Content::SkinDefinition::MaxJoints * sizeof(KT::Core::Math::Matrix4);
		if (world.objects.size() > (maxCapacity - 768) / bytesPerDraw)
		{
			throw std::overflow_error("静的Sceneの描画数が定数領域の上限を超えています。");
		}
		std::size_t required = 768;
		for (const auto& object : world.objects)
		{
			required += 512 + (object.palette ? bytesPerDraw - 512 : 0);
		}
		if (view &&
			(!depth_ || depth_->GetTexture().GetWidth() != color.GetWidth() || depth_->GetTexture().GetHeight() != color.GetHeight()))
		{
			// 古いCommandListも破棄してから、次の寸法の深度画像へ置き換える。
			ReleaseFrame();
			auto depth = std::make_unique<KT::Graphics::DepthBuffer>(device_, color.GetWidth(), color.GetHeight());
			depth_ = std::move(depth);
		}
		const bool toon = std::any_of(
			world.objects.begin(), world.objects.end(), [this](const auto& object) { return materials_.Get(object.materialId).IsToon(); });
		if (view && (toon || lighting.shadows) && (!shadow_ || shadow_->GetTexture().GetWidth() != lighting.shadowResolution))
		{
			// 旧ListとtableのGPU利用を完了させ、失敗した候補では旧画像を維持する。
			ReleaseFrame();
			auto pending = std::make_unique<KT::Graphics::DepthBuffer>(
				device_, lighting.shadowResolution, lighting.shadowResolution, DXGI_FORMAT_D32_FLOAT, true);
			materials_.BindShadow(KT::Graphics::PixelDepthView(*pending));
			shadow_ = std::move(pending);
		}
		if (!frame_ || constantCapacity_ < required)
		{
			const auto grown = (std::min)(maxCapacity, (std::max)(std::size_t{65536}, constantCapacity_ + constantCapacity_ / 2));
			const auto capacity = constantCapacity_ >= required ? constantCapacity_ : (std::max)(required, grown) & ~std::size_t{255};
			auto frame = std::make_unique<KT::Graphics::FrameResources>(device_, queue_, capacity);
			frame_ = std::move(frame);
			constantCapacity_ = capacity;
		}

		try
		{
			if (effects && view)
			{
				// 外部描画poolの再利用をGraph外で準備し、記録callback中は待機しない。
				effects->PrepareFrame(queue_);
			}
			KT::Renderer::RenderGraph graph;
			const auto output = graph.ImportTexture({"SceneColor", color.GetResource(), colorState, colorState, false, true});
			const auto colorView = graph.AddView({"SceneColorRTV", output, {}, color});
			AddClearPass(graph, colorView, {0, 0, 0, 1});
			frame_->Begin();
			if (view)
			{
				const auto depth = graph.ImportTexture({"SceneDepth", depth_->GetTexture().GetResource(), D3D12_RESOURCE_STATE_COMMON,
					D3D12_RESOURCE_STATE_COMMON, false, true});
				const auto depthView = graph.AddView({"SceneDepthDSV", depth, {}, KT::Graphics::DepthTargetView(*depth_)});
				AddDepthClearPass(graph, depthView);
				const auto prepared = PreparedRenderFrame::Prepare(world, *view, meshes_, materials_, frame_->GetConstants(), lighting);
				GraphViewHandle shadowRead;
				if (toon || lighting.shadows)
				{
					const auto resource = graph.ImportTexture({"DirectionalShadow", shadow_->GetTexture().GetResource(),
						D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON, false, true});
					const auto shadowDepth = graph.AddView({"ShadowDSV", resource, {}, KT::Graphics::DepthTargetView(*shadow_)});
					AddDepthClearPass(graph, shadowDepth);
					if (lighting.shadows)
					{
						const ViewConstants shadowView{lighting.constants.shadowViewProjection};
						const auto slice = frame_->GetConstants().Write(std::as_bytes(std::span{&shadowView, 1}));
						AddShadowPass(graph, shadowDepth, prepared, slice, lighting.shadowResolution);
					}
					if (toon)
					{
						shadowRead = graph.AddView({"ShadowSRV", resource, {}, KT::Graphics::PixelDepthView(*shadow_)});
					}
				}
				AddOpaquePass(graph, colorView, depthView, prepared, shadowRead);
				if (effects && effects->PlayingCount() != 0)
				{
					// 同じ深度のreadonly viewで判定し、色だけへエフェクトを重ねる。
					const auto effectDepth = graph.AddView({"EffectDepthDSV", depth, {}, KT::Graphics::DepthTargetView(*depth_, true)});
					AddEffectPass(graph, colorView, effectDepth, *effects, *view);
				}
			}
			if (ui)
			{
				AddUiPass(graph, colorView, *ui);
			}
			graph.Compile();
			graph.Record(*frame_);
			frame_->EndRecording();
			frame_->Submit();
			lastDrawCount_ = world.objects.size();
			++submittedFrames_;
		}
		catch (...)
		{
			// 送信前の失敗なら記録を破棄する。成功済みGPU素材は再試行で再利用する。
			ReleaseFrame();
			throw;
		}
	}

	void StaticSceneRenderer::Wait()
	{
		if (frame_)
		{
			frame_->Wait();
		}
	}

	void StaticSceneRenderer::ReleaseFrame()
	{
		Wait();
		frame_.reset();
	}

	std::size_t StaticSceneRenderer::MeshCount() const noexcept
	{
		return meshIds_.size();
	}

	std::size_t StaticSceneRenderer::MaterialCount() const noexcept
	{
		return materialIds_.size();
	}

	std::size_t StaticSceneRenderer::LastDrawCount() const noexcept
	{
		return lastDrawCount_;
	}

	std::uint64_t StaticSceneRenderer::SubmittedFrames() const noexcept
	{
		return submittedFrames_;
	}
}
