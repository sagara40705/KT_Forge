#include <Graphics/FrameResources.h>
#include <Graphics/GraphicsValidation.h>
#include <exception>
#include <stdexcept>

namespace KT::Graphics
{
	FrameResources::FrameResources(GraphicsDevice& device, CommandQueue& queue, std::size_t capacity)
		: queue_(queue),context_(device),constants_(device,capacity)
	{
		RequireSameDevice(queue.GetCommandQueue(),device.GetDevice());
		context_.frameOwned_=true;
		context_.frameConstants_=&constants_;
	}
	FrameResources::~FrameResources()
	{
		try { Wait(); } catch (...) { std::terminate(); }
	}
	void FrameResources::Begin()
	{
		if (state_==State::Recording || state_==State::Ready || state_==State::Failed)
			throw std::logic_error("Frame Begin requires idle/submitted slot.");
		Wait();
		try { context_.frameBegin_=true; context_.Begin(); constants_.Begin(); state_=State::Recording; }
		catch (...) { constants_.Seal(); context_.Invalidate(); state_=State::Failed; throw; }
	}
	void FrameResources::EndRecording()
	{
		if (state_!=State::Recording) throw std::logic_error("Frame End requires recording.");
		try { context_.End(); constants_.Seal(); state_=State::Ready; }
		catch (...) { constants_.Seal(); context_.Invalidate(); state_=State::Failed; throw; }
	}
	std::uint64_t FrameResources::Submit()
	{
		if (state_!=State::Ready) throw std::logic_error("Frame Submit requires ended recording.");
		// Listの有効性は送信前に確認。送信後のSignal等の失敗は回復/資源解放できないためfatal。
		context_.frameSubmit_=true;
		try { (void)context_.GetExecutableList(); }
		catch (...) { context_.frameSubmit_=false; state_=State::Failed; throw; }
		try { queue_.Execute(context_); context_.frameSubmit_=false; fence_=queue_.Signal(); state_=State::Submitted; return fence_; }
		catch (...) { std::terminate(); }
	}
	void FrameResources::Wait()
	{
		if (state_!=State::Submitted) return;
		try { queue_.Wait(fence_); state_=State::Idle; }
		catch (...) { std::terminate(); }
	}
	CommandContext& FrameResources::GetContext()
	{
		if (state_!=State::Recording) throw std::logic_error("Frame Context is only borrowed while recording.");
		return context_;
	}
}
