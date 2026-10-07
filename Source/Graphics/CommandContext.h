#pragma once
#include <Core/Utility/NonCopyable.h>
#include<Graphics/GraphicsDevice.h>

namespace KT::Graphics
{
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
		// コマンドリストの記録を開始
		void Begin();
		// コマンドリストの記録を終了
		void End();
	private:
		// 記録中か
		bool recording_ = false;

	public:
		// 記録中のCommandListを取得
		ID3D12GraphicsCommandList* GetRecordingList() const;
		// 実行可能なCommandListを取得
		ID3D12CommandList* GetExecutableList() const;
	};
}
