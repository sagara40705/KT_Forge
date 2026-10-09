#pragma once
#include <Graphics/CommandContext.h>
#include <Graphics/CommandQueue.h>
#include <Graphics/ConstantBufferArena.h>

namespace KT::Graphics
{
	// 1slot: allocator/list/定数arenaを所有、queueを非所有借用。queueはこのobjectより長生きさせる。
	// Beginは前submitのFenceを待ってからreset。EndRecordingで定数を書込禁止、Submitでfenceを発行。
	// GPUが借りるMesh・Material・画像・heapもWait完了まで外部所有者が保持する。
	class FrameResources : private KT::Core::NonCopyable
	{
	public:
		FrameResources(GraphicsDevice& device, CommandQueue& queue, std::size_t constantCapacity=65536);
		~FrameResources();
		void Begin();
		void EndRecording();
		std::uint64_t Submit();
		void Wait();
		CommandContext& GetContext();
		ConstantBufferArena& GetConstants() noexcept { return constants_; }
	private:
		enum class State { Idle, Recording, Ready, Submitted, Failed };
		CommandQueue& queue_;
		CommandContext context_;
		ConstantBufferArena constants_;
		std::uint64_t fence_=0;
		State state_=State::Idle;
	};
}
