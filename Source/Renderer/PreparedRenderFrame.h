#pragma once
#include <Graphics/Buffers/ConstantBufferArena.h>
#include <d3d12.h>
#include <cstddef>
#include <vector>
#include <Graphics/Commands/IndexedDrawPacket.h>
#include <Renderer/Lighting/SceneLighting.h>

namespace KT::Graphics
{
	class CommandContext;
	class ColorTargetView;
	class DepthBuffer;
}

namespace KT::Renderer
{
	class Mesh;
	class UnlitMaterial;
	class MeshStore;
	class MaterialStore;
	struct RenderWorld;
	struct RenderView;

	struct PreparedDraw
	{
		const Mesh* mesh = nullptr;
		const UnlitMaterial* material = nullptr;
		KT::Graphics::ConstantSlice objectConstants, materialConstants;
		KT::Graphics::IndexedDrawRange range;
		// Skin Meshだけで使う個体別CBV。判定はMeshのbinding数で行う。
		KT::Graphics::ConstantSlice skinConstants;
		// Toonでのみ使うMaterialStore所有の二画像table。
		const KT::Graphics::ColorDepthTextureTable* texturePair = nullptr;
	};

	// CPU RenderWorld/Viewの値snapshotをID解決し、現在frameのGPU定数を用意する。
	// MeshStore/MaterialStore/arenaは非所有借用。GPU完了まで保持、次epochでは作り直す。
	// Graph/Worldを知らず、pipeline組立/clear/barrier/submitは行わない。
	class PreparedRenderFrame
	{
	public:
		static PreparedRenderFrame Prepare(const RenderWorld& world, const RenderView& view, const MeshStore& meshes,
			const MaterialStore& materials, KT::Graphics::ConstantBufferArena& constants, const SceneLighting& lighting = {});
		void RecordDraw(KT::Graphics::CommandContext& context, std::size_t index, const KT::Graphics::ColorTargetView& target,
			const KT::Graphics::DepthBuffer& depth, const KT::Graphics::ConstantBufferArena& constants) const;
		// Prepareで保持した定数sliceと描画寸法をOpaquePassへ渡す。
		KT::Graphics::ConstantSlice GetViewConstants() const noexcept;

		KT::Graphics::ConstantSlice GetLightConstants() const noexcept
		{
			return lightConstants_;
		}
		UINT GetWidth() const noexcept;
		UINT GetHeight() const noexcept;

		const std::vector<PreparedDraw>& GetDraws() const noexcept
		{
			return draws_;
		}

	private:
		KT::Graphics::ConstantSlice viewConstants_;
		// Toonを使うFrameでだけ書き込むb4。
		KT::Graphics::ConstantSlice lightConstants_;
		std::vector<PreparedDraw> draws_;
		UINT width_ = 0, height_ = 0;
	};
}
