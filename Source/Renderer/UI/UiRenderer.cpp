#include <Renderer/UI/UiRenderer.h>
#include <Graphics/Buffers/GpuBuffer.h>
#include <Graphics/Textures/SampledTexture.h>
#include <Graphics/Textures/ColorTargetView.h>
#include <Graphics/Descriptors/Sampler.h>
#include <Graphics/Commands/CommandContext.h>
#include <Graphics/Commands/CommandQueue.h>
#include <Graphics/GraphicsValidation.h>
#include <Content/TextureLoader.h>
#include <Core/Math/Matrix4.h>
#include <RmlUi/Core.h>
#include <Ui2DVS.h>
#include <Ui2DPS.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <stdexcept>

namespace KT::Renderer
{
	namespace G = KT::Graphics;

	// 上流のgeometry/texture handleとGraphへ渡すsnapshotを管理する。
	struct UiRenderer::Impl : Rml::RenderInterface
	{
		struct Geometry
		{
			// 不変UPLOAD Buffer。GENERIC_READのままIAへ借用する。
			G::GpuBuffer vertices, indices;
			UINT indexCount;

			Geometry(G::GraphicsDevice& device, Rml::Span<const Rml::Vertex> vertexData, Rml::Span<const int> indexData)
				: vertices(device, std::as_bytes(std::span(vertexData.data(), vertexData.size()))),
				  indices(device, std::as_bytes(std::span(indexData.data(), indexData.size()))),
				  indexCount(static_cast<UINT>(indexData.size()))
			{
			}
		};

		// shared_ptrで捕捉し、上流Release後も最後の描画Fenceまで実体を保持する。
		struct Draw
		{
			std::shared_ptr<Geometry> geometry;
			std::shared_ptr<G::SampledTexture> texture;
			std::array<float, 20> constants;
			D3D12_RECT scissor;
		};

		G::GraphicsDevice& device;
		G::CommandQueue& queue;
		DXGI_FORMAT format;
		// UI専用Bindingとpremultiplied alpha PSO。既存Materialと共有しない。
		G::ComPtr<ID3D12RootSignature> root;
		G::ComPtr<ID3D12PipelineState> pipeline;
		G::Sampler sampler;
		std::shared_ptr<G::SampledTexture> white;
		// 上流handleから所有実体を引く。Release後の描画snapshotはdrawsで保持する。
		std::unordered_map<Rml::CompiledGeometryHandle, std::shared_ptr<Geometry>> geometries;
		std::unordered_map<Rml::TextureHandle, std::shared_ptr<G::SampledTexture>> textures;
		std::vector<Draw> draws;
		// 0は上流の無効値。解放済み番号を再利用しない。
		std::uintptr_t nextGeometry = 1, nextTexture = 1;
		unsigned width = 0, height = 0;
		// preparedはEndCaptureで確定し、Record一回で消費する。
		bool capturing = false, prepared = false, scissorEnabled = false;
		D3D12_RECT scissor{};
		KT::Core::Math::Matrix4 transform;

		Impl(G::GraphicsDevice& graphics, G::CommandQueue& commands, DXGI_FORMAT targetFormat)
			: device(graphics),
			  queue(commands),
			  format(targetFormat),
			  sampler(graphics, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP)
		{
			G::RequireSameDevice(queue.GetCommandQueue(), device.GetDevice());
			if (!G::IsColorTargetFormat(format))
			{
				throw std::invalid_argument("UIの出力formatが未対応です。");
			}
			// UIは深度なし・premultiplied alpha、同じDirect Listの末尾へ合成する。
			const D3D12_DESCRIPTOR_RANGE ranges[]{
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0}, {D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0, 0}};
			std::array<D3D12_ROOT_PARAMETER, 3> parameters{};
			parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			parameters[0].Constants = {0, 0, 20};
			parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			for (UINT index = 0; index < 2; ++index)
			{
				parameters[index + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				parameters[index + 1].DescriptorTable = {1, &ranges[index]};
				parameters[index + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			}
			const D3D12_ROOT_SIGNATURE_DESC description{
				3, parameters.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
			G::ComPtr<ID3DBlob> serialized, error;
			G::CheckGraphicsResult(
				D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1, serialized.GetAddressOf(), error.GetAddressOf()),
				"UI RootSignatureのSerializeに失敗しました。");
			G::CheckGraphicsResult(device.GetDevice()->CreateRootSignature(
									   0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(root.GetAddressOf())),
				"UI RootSignatureの生成に失敗しました。");
			static_assert(sizeof(Rml::Vertex) == 20 && sizeof(int) == 4);
			const D3D12_INPUT_ELEMENT_DESC layout[]{
				{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
			D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};
			state.pRootSignature = root.Get();
			state.VS = {Ui2DVS, sizeof(Ui2DVS)};
			state.PS = {Ui2DPS, sizeof(Ui2DPS)};
			state.InputLayout = {layout, 3};
			state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			state.SampleMask = UINT_MAX;
			state.SampleDesc.Count = 1;
			state.NumRenderTargets = 1;
			state.RTVFormats[0] = format;
			state.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
			state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			state.RasterizerState.DepthClipEnable = TRUE;
			auto& blend = state.BlendState.RenderTarget[0];
			blend.BlendEnable = TRUE;
			blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
			blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
			blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			state.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			state.DepthStencilState.FrontFace.StencilFunc = state.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			state.DepthStencilState.FrontFace.StencilFailOp = state.DepthStencilState.FrontFace.StencilDepthFailOp =
				state.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			state.DepthStencilState.BackFace = state.DepthStencilState.FrontFace;
			G::CheckGraphicsResult(device.GetDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(pipeline.GetAddressOf())),
				"UI Pipelineの生成に失敗しました。");
			const std::array<std::uint8_t, 4> pixel{255, 255, 255, 255};
			const G::TextureMipData mip{1, 1, pixel};
			white = std::make_shared<G::SampledTexture>(device, std::span(&mip, 1), DXGI_FORMAT_R8G8B8A8_UNORM);
			white->Upload(queue);
		}

		Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override
		{
			// 全入力を確認してから、新しい不変Bufferを所有する。
			if (vertices.empty() || indices.empty() || indices.size() % 3 != 0 || indices.size() > UINT_MAX / sizeof(int) ||
				vertices.size() > UINT_MAX / sizeof(Rml::Vertex) || nextGeometry == UINTPTR_MAX)
			{
				throw std::invalid_argument("UI geometryの頂点数・三角形数またはhandleが上限外です。");
			}
			for (const auto& vertex : vertices)
			{
				if (!std::isfinite(vertex.position.x) || !std::isfinite(vertex.position.y) || !std::isfinite(vertex.tex_coord.x) ||
					!std::isfinite(vertex.tex_coord.y))
				{
					throw std::invalid_argument("UI geometryの位置またはUVが有限値ではありません。");
				}
			}
			for (const auto index : indices)
			{
				if (index < 0 || static_cast<std::size_t>(index) >= vertices.size())
				{
					throw std::invalid_argument("UI geometryのindexが頂点範囲外です。");
				}
			}
			auto geometry = std::make_shared<Geometry>(device, vertices, indices);
			const auto handle = nextGeometry++;
			geometries.emplace(handle, std::move(geometry));
			return handle;
		}

		void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override
		{
			if (!capturing || !std::isfinite(translation.x) || !std::isfinite(translation.y))
			{
				throw std::logic_error("UI描画列はGraph記録前の捕捉中だけ生成できます。");
			}
			auto clip = scissorEnabled ? scissor : D3D12_RECT{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
			clip.left = (std::clamp)(clip.left, 0L, static_cast<LONG>(width));
			clip.right = (std::clamp)(clip.right, clip.left, static_cast<LONG>(width));
			clip.top = (std::clamp)(clip.top, 0L, static_cast<LONG>(height));
			clip.bottom = (std::clamp)(clip.bottom, clip.top, static_cast<LONG>(height));
			if (clip.left == clip.right || clip.top == clip.bottom)
			{
				return;
			}
			Draw draw{geometries.at(geometry), texture ? textures.at(texture) : white, {}, clip};
			std::copy(transform.elements.begin(), transform.elements.end(), draw.constants.begin());
			draw.constants[16] = static_cast<float>(width);
			draw.constants[17] = static_cast<float>(height);
			draw.constants[18] = translation.x;
			draw.constants[19] = translation.y;
			draws.push_back(std::move(draw));
		}

		void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override
		{
			geometries.erase(geometry);
		}

		Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i size) override
		{
			if (size.x <= 0 || size.y <= 0 || size.x > 16384 || size.y > 16384 ||
				source.size() != static_cast<std::size_t>(size.x) * size.y * 4 || nextTexture == UINTPTR_MAX)
			{
				throw std::invalid_argument("UI Textureの寸法・RGBA8 byte数またはhandleが上限外です。");
			}
			// 上流のpremultiplied sRGBを一度straightへ戻し、線形premultiplied RGBA8へ変換する。
			std::vector<std::uint8_t> pixels(source.begin(), source.end());
			for (std::size_t index = 0; index < pixels.size(); index += 4)
			{
				const float alpha = pixels[index + 3] / 255.0f;
				for (std::size_t channel = 0; channel < 3; ++channel)
				{
					const float value = alpha > 0 ? (std::min)(1.0f, pixels[index + channel] / (255.0f * alpha)) : 0;
					const float linear = value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
					pixels[index + channel] = static_cast<std::uint8_t>(std::lround(linear * alpha * 255));
				}
			}
			const G::TextureMipData mip{static_cast<UINT>(size.x), static_cast<UINT>(size.y), pixels};
			auto image = std::make_shared<G::SampledTexture>(device, std::span(&mip, 1), DXGI_FORMAT_R8G8B8A8_UNORM);
			image->Upload(queue);
			const auto handle = nextTexture++;
			textures.emplace(handle, std::move(image));
			return handle;
		}

		Rml::TextureHandle LoadTexture(Rml::Vector2i& size, const Rml::String& source) override
		{
			const auto path = std::filesystem::u8path(source);
			auto asset = KT::Content::TextureLoader::Load(
				KT::Content::TextureAsset(source, KT::Content::TextureColorSpace::Srgb, KT::Content::TextureMipMode::Preserve), path);
			const auto& mip = asset.Mips().front();
			auto pixels = mip.pixels;
			// ファイル画像はstraight alphaなので、上流のGenerateTexture契約へ合わせる。
			for (std::size_t index = 0; index < pixels.size(); index += 4)
			{
				for (std::size_t channel = 0; channel < 3; ++channel)
				{
					pixels[index + channel] =
						static_cast<std::uint8_t>((static_cast<unsigned>(pixels[index + channel]) * pixels[index + 3] + 127) / 255);
				}
			}
			size = {static_cast<int>(mip.width), static_cast<int>(mip.height)};
			return GenerateTexture({pixels.data(), pixels.size()}, size);
		}

		void ReleaseTexture(Rml::TextureHandle texture) override
		{
			textures.erase(texture);
		}

		void EnableScissorRegion(bool enabled) override
		{
			scissorEnabled = enabled;
		}

		void SetScissorRegion(Rml::Rectanglei region) override
		{
			scissor = {region.Left(), region.Top(), region.Right(), region.Bottom()};
		}

		void SetTransform(const Rml::Matrix4f* matrix) override
		{
			transform = {};
			if (matrix)
			{
				// 上流column-vectorを転置し、CPU/HLSLのrow-vectorへ統一する。
				for (int row = 0; row < 4; ++row)
				{
					for (int column = 0; column < 4; ++column)
					{
						transform(row, column) = matrix->GetRow(column)[row];
					}
				}
				if (!KT::Core::Math::IsFinite(transform))
				{
					throw std::invalid_argument("UI Transformが有限値ではありません。");
				}
			}
		}

		void EnableClipMask(bool enable) override
		{
			if (enable)
			{
				throw std::invalid_argument("初版UIは矩形clipだけに対応します。丸角・変形要素のmaskは未対応です。");
			}
		}

		Rml::LayerHandle PushLayer() override
		{
			throw std::invalid_argument("初版UIはfilter・opacity layer・背景ぼかしに対応していません。");
		}

		Rml::CompiledShaderHandle CompileShader(const Rml::String&, const Rml::Dictionary&) override
		{
			throw std::invalid_argument("初版UIはgradientなどの追加decorator shaderに対応していません。");
		}
	};

	UiRenderer::UiRenderer(G::GraphicsDevice& device, G::CommandQueue& queue, DXGI_FORMAT format)
		: impl_(std::make_unique<Impl>(device, queue, format))
	{
	}

	UiRenderer::~UiRenderer()
	{
		impl_->queue.Wait(impl_->queue.Signal());
	}

	Rml::RenderInterface& UiRenderer::Interface() noexcept
	{
		return *impl_;
	}

	void UiRenderer::BeginCapture(unsigned width, unsigned height)
	{
		if (width == 0 || height == 0 || width > 16384 || height > 16384 || impl_->capturing)
		{
			throw std::invalid_argument("UI捕捉の寸法は1..16384とし、再入しないでください。");
		}
		impl_->queue.Wait(impl_->queue.Signal());
		impl_->draws.clear();
		impl_->width = width;
		impl_->height = height;
		impl_->scissorEnabled = false;
		impl_->transform = {};
		impl_->prepared = false;
		impl_->capturing = true;
	}

	void UiRenderer::EndCapture()
	{
		if (!impl_->capturing)
		{
			throw std::logic_error("UI捕捉が開始されていません。");
		}
		impl_->capturing = false;
		impl_->prepared = true;
	}

	void UiRenderer::Record(G::CommandContext& commands, const G::ColorTargetView& color)
	{
		try
		{
			if (!impl_->prepared || color.GetFormat() != impl_->format || color.GetWidth() != impl_->width ||
				color.GetHeight() != impl_->height)
			{
				throw std::invalid_argument("UI捕捉済み状態・RTV format・描画寸法が一致しません。");
			}
			auto* list = commands.GetRecordingList();
			G::RequireSameDevice(list, impl_->device.GetDevice());
			G::RequireSameDevice(color.GetResource(), impl_->device.GetDevice());
			impl_->prepared = false;
			const auto target = color.GetRtv();
			list->OMSetRenderTargets(1, &target, FALSE, nullptr);
			const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(impl_->width), static_cast<float>(impl_->height), 0, 1};
			list->RSSetViewports(1, &viewport);
			list->SetGraphicsRootSignature(impl_->root.Get());
			list->SetPipelineState(impl_->pipeline.Get());
			list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			for (const auto& draw : impl_->draws)
			{
				const D3D12_VERTEX_BUFFER_VIEW vertices{draw.geometry->vertices.GetResource()->GetGPUVirtualAddress(),
					static_cast<UINT>(draw.geometry->vertices.GetSize()), sizeof(Rml::Vertex)};
				const D3D12_INDEX_BUFFER_VIEW indices{draw.geometry->indices.GetResource()->GetGPUVirtualAddress(),
					static_cast<UINT>(draw.geometry->indices.GetSize()), DXGI_FORMAT_R32_UINT};
				ID3D12DescriptorHeap* heaps[]{draw.texture->GetHeap(), impl_->sampler.GetHeap()};
				list->SetDescriptorHeaps(2, heaps);
				list->SetGraphicsRoot32BitConstants(0, 20, draw.constants.data(), 0);
				list->SetGraphicsRootDescriptorTable(1, draw.texture->GetSrv());
				list->SetGraphicsRootDescriptorTable(2, impl_->sampler.GetSampler());
				list->RSSetScissorRects(1, &draw.scissor);
				list->IASetVertexBuffers(0, 1, &vertices);
				list->IASetIndexBuffer(&indices);
				list->DrawIndexedInstanced(draw.geometry->indexCount, 1, 0, 0, 0);
			}
		}
		catch (...)
		{
			commands.Invalidate();
			throw;
		}
	}

	void UiRenderer::AbortCapture() noexcept
	{
		impl_->capturing = false;
		impl_->prepared = false;
	}

	std::size_t UiRenderer::DrawCount() const noexcept
	{
		return impl_->draws.size();
	}
}
