#pragma once
#include <Graphics/Commands/CommandContext.h>
#include <Graphics/Commands/CommandQueue.h>
#include <Graphics/Buffers/ConstantBufferArena.h>

namespace KT::Graphics
{
	// 1slot: allocator/list/定数arenaを所有、queueを非所有借用。queueはこのobjectより長生きさせる。
	// Beginは前submitのFenceを待ってからreset。EndRecordingで定数を書込禁止、Submitでfenceを発行。
	// GPUが借りるMesh・Material・画像・heapもWait完了まで外部所有者が保持する。
	// 記録前・記録中の失敗は例外を返す。送信後のSignal/Wait失敗は原因を診断して終了する。
	class FrameResources : private KT::Core::NonCopyable
	{
	public:
		FrameResources(GraphicsDevice& device, CommandQueue& queue, std::size_t constantCapacity = 65536);
		~FrameResources();
		void Begin();
		void EndRecording();
		std::uint64_t Submit();
		void Wait();
		CommandContext& GetContext();

		ConstantBufferArena& GetConstants() noexcept
		{
			return constants_;
		}

	private:
		// Failedは未送信の記録失敗。GPU同期失敗はこの状態へ戻さず診断終了する。
		enum class State
		{
			Idle,
			Recording,
			Ready,
			Submitted,
			Failed
		};
		// GPU利用完了まで生存する所有者から借用する。
		CommandQueue& queue_;
		CommandContext context_;
		ConstantBufferArena constants_;
		// このslotを最後に送信した完了目印。
		std::uint64_t fence_ = 0;
		State state_ = State::Idle;
	};
}
