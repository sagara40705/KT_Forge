#pragma once
#include <Content/AssetCatalog.h>
#include <Renderer/MeshRenderCandidate.h>
#include <Renderer/RenderData.h>
#include <Renderer/MeshStore.h>
#include <Renderer/MaterialStore.h>
#include <Graphics/Commands/FrameResources.h>
#include <Graphics/Textures/ColorTargetView.h>
#include <Graphics/Textures/DepthBuffer.h>
#include <optional>

namespace KT::Renderer
{
	// 静的MeshのGPU実体・深度・同期1slotを所有する。WorldとEntityには依存しない。
	// Device/Queue/固定Catalogはこの所有者より長く保持する。
	class StaticSceneRenderer : private KT::Core::NonCopyable
	{
	public:
		StaticSceneRenderer(
			KT::Graphics::GraphicsDevice& device, KT::Graphics::CommandQueue& queue, const KT::Content::AssetCatalog& assets);
		// 完成したCPU値からClear/DepthClear/Opaqueを送信する。CameraなしならClearだけ。
		// colorは同一DeviceのRGBA8、colorStateは描画前後で同じ既知stateとする。
		// targetの所有者はWaitまで保持する。Presentは呼出側がSubmit後に一度だけ行う。
		void Submit(const RenderCandidates& candidates, const std::optional<RenderView>& view, const KT::Graphics::ColorTargetView& color,
			D3D12_RESOURCE_STATES colorState = D3D12_RESOURCE_STATE_PRESENT);
		void Wait();
		// Resizeや外部画像破棄の前に呼び、Fence完了後に旧CommandListの参照も破棄する。
		void ReleaseFrame();
		[[nodiscard]] std::size_t MeshCount() const noexcept;
		[[nodiscard]] std::size_t MaterialCount() const noexcept;
		[[nodiscard]] std::size_t LastDrawCount() const noexcept;
		[[nodiscard]] std::uint64_t SubmittedFrames() const noexcept;

	private:
		std::uint64_t ResolveMesh(KT::Content::MeshAssetId id);
		std::uint64_t ResolveMaterial(KT::Content::MaterialAssetId id);
		KT::Graphics::GraphicsDevice& device_;
		KT::Graphics::CommandQueue& queue_;
		const KT::Content::AssetCatalog& assets_;
		MeshStore meshes_;
		MaterialStore materials_;
		// UUIDとStore内handleを分離する。成功した実体だけを対応付け、削除・reloadは行わない。
		std::unordered_map<KT::Content::AssetId, std::uint64_t, KT::Content::AssetIdHash> meshIds_;
		std::unordered_map<KT::Content::AssetId, std::uint64_t, KT::Content::AssetIdHash> materialIds_;
		std::unique_ptr<KT::Graphics::DepthBuffer> depth_;
		std::size_t constantCapacity_ = 0;
		std::size_t lastDrawCount_ = 0;
		std::uint64_t submittedFrames_ = 0;
		// 最後に宣言し、GPUが借りる素材・深度より先に完了待機して破棄する。
		std::unique_ptr<KT::Graphics::FrameResources> frame_;
	};
}
