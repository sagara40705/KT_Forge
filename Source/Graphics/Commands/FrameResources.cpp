#include <Graphics/Commands/FrameResources.h>
#include <Graphics/GraphicsValidation.h>
#include <exception>
#include <stdexcept>

namespace KT::Graphics
{
	FrameResources::FrameResources(GraphicsDevice& device, CommandQueue& queue, std::size_t capacity)
		: queue_(queue),
		  context_(device),
		  constants_(device, capacity)
	{
		// QueueとContextを同じDeviceのFrameとして関連付ける
		RequireSameDevice(queue.GetCommandQueue(), device.GetDevice());
		context_.frameOwned_ = true;
		context_.frameConstants_ = &constants_;
	}

	FrameResources::~FrameResources()
	{
		try
		{
			Wait();
		}
		catch (...)
		{
			std::terminate();
		}
	}

	// 前回のFence完了後に、allocator/list/定数arenaを次の記録へ再利用する。
	void FrameResources::Begin()
	{
		if (state_ == State::Recording || state_ == State::Ready || state_ == State::Failed)
		{
			throw std::logic_error("Frame Begin requires idle/submitted slot.");
		}
		// 再利用条件：前回送信した命令が使う領域を、GPU完了前に上書きしない。
		Wait();
		try
		{
			// Contextと定数領域の次の記録を開始する
			context_.frameBegin_ = true;
			context_.Begin();
			constants_.Begin();
			state_ = State::Recording;
		}
		catch (...)
		{
			constants_.Seal();
			context_.Invalidate();
			state_ = State::Failed;
			throw;
		}
	}

	// listを閉じて定数を書込禁止にする。失敗したContextは送信可能状態へ進めない。
	void FrameResources::EndRecording()
	{
		if (state_ != State::Recording)
		{
			throw std::logic_error("Frame End requires recording.");
		}
		try
		{
			// Listを閉じて定数への書込みを禁止する
			context_.End();
			constants_.Seal();
			state_ = State::Ready;
		}
		catch (...)
		{
			constants_.Seal();
			context_.Invalidate();
			state_ = State::Failed;
			throw;
		}
	}

	// 記録を送信してFenceを発行する。送信後の同期失敗は資源を安全に解放できないため停止する。
	std::uint64_t FrameResources::Submit()
	{
		if (state_ != State::Ready)
		{
			throw std::logic_error("Frame Submit requires ended recording.");
		}
		// Listの有効性は送信前に確認。送信後のSignal等の失敗は回復/資源解放できないためfatal。
		context_.frameSubmit_ = true;
		try
		{
			(void)context_.GetExecutableList();
		}
		catch (...)
		{
			context_.frameSubmit_ = false;
			state_ = State::Failed;
			throw;
		}
		try
		{
			// 命令を送信して完了待機に使うFenceを発行する
			queue_.Execute(context_);
			context_.frameSubmit_ = false;
			fence_ = queue_.Signal();
			state_ = State::Submitted;
			return fence_;
		}
		catch (...)
		{
			std::terminate();
		}
	}

	// 送信済みFenceの完了を待ち、slotを再利用できる状態へ戻す。
	void FrameResources::Wait()
	{
		if (state_ != State::Submitted)
		{
			return;
		}
		try
		{
			// 送信したFenceの完了後にslotを再利用可能にする
			queue_.Wait(fence_);
			state_ = State::Idle;
		}
		catch (...)
		{
			std::terminate();
		}
	}

	// 記録中だけContextを借用させる。Context自身の健全性は各記録入口でも確認する。
	CommandContext& FrameResources::GetContext()
	{
		if (state_ != State::Recording)
		{
			throw std::logic_error("Frame Context is only borrowed while recording.");
		}
		return context_;
	}
}
