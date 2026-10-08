#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Graphics/GraphicsDevice.h>
#include <array>
#include <span>

namespace KT::Graphics
{
	class FrameResources;
	class GraphicsPipelineState;
	class VertexBuffer;
	class IndexBuffer;
	class ColorTargetView;
	class DepthBuffer;
	class ConstantBufferArena;
	struct RootConstantBinding;

	// AllocatorとListを所有。「命令の記録」を担当する
	class CommandContext : private KT::Core::NonCopyable
	{
	public:
		// コンストラクタ
		explicit CommandContext(GraphicsDevice& device);

		// デストラクタ
		~CommandContext() = default;

	private:
		ComPtr<ID3D12CommandAllocator> commandAllocator_{};
		ComPtr<ID3D12GraphicsCommandList> commandList_{};

	public:
		// 記録を開始する。呼出側はこのAllocatorを使った前回のGPU処理完了を確認する
		void Begin();
		// コマンドリストの記録を終了
		void End();
	private:
		// 記録中か
		bool recording_ = false;
		// GPU処理が失敗したか
		bool failed_ = false;
		friend class FrameResources;
		bool frameOwned_ = false, frameBegin_ = false, frameSubmit_ = false;
		const ConstantBufferArena* frameConstants_ = nullptr;

	public:
		// 記録中のCommandListを取得
		ID3D12GraphicsCommandList* GetRecordingList() const;
		// 実行可能なCommandListを取得
		ID3D12CommandList* GetExecutableList() const;

	public:
		// リソースの状態を遷移する
		void Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

		// 全域Clearを記録する。呼出側は対象がRENDER_TARGET状態であることを保証する
		void ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, const std::array<float, 4>& color);

		// 有効なowner-backed画像/同一device/定数tokenを検査し、全indexを記録する。
		// 呼出側がRT=RENDER_TARGET、depth=DEPTH_WRITEを保証。barrier/clear/submitなし。
		// 同じFrameResourcesのarenaだけを受理。standalone ContextからのDrawは拒否。
		// すべての借用部品はGPU完了まで保持。失敗時Invalidate、以後送信禁止。
		void DrawIndexed(const GraphicsPipelineState& pipeline, const VertexBuffer& vertices, const IndexBuffer& indices,
			const ColorTargetView& target, const DepthBuffer& depth, const ConstantBufferArena& constants,
			std::span<const RootConstantBinding> bindings);
		void ClearDepth(const DepthBuffer& depth); // Reverse-Z全域clear0。DEPTH_WRITEは呼出側。
		// 記録失敗時にContextを使用禁止にする。GPU待機や命令の取り消しは行わない
		void Invalidate() noexcept;
	};
}
