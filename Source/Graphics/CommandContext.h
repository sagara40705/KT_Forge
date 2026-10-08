#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Graphics/GraphicsDevice.h>
#include <array>

namespace KT::Graphics
{
	class TriangleRenderer;

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

		// 三角形を記録する。RTVはRendererと同じformat、MSAAなし、指定サイズの
		// RENDER_TARGET状態であることを呼出側が保証する。Clear・遷移・提出はしない。
		// RendererとRTV heap/画像はGPU完了まで保持する。Listの描画状態は復元しない。
		void DrawTriangle(const TriangleRenderer& renderer, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
			UINT width, UINT height);

		// 記録失敗時にContextを使用禁止にする。GPU待機や命令の取り消しは行わない
		void Invalidate() noexcept;
	};
}
