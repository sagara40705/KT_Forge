#pragma once
#include <Graphics/DescriptorHeap.h>

namespace KT::Graphics
{
	// heapを非所有借用、heap毎に1個。初版は単一threadの単調割当、個別free/resetなし。
	// GPUが使うdescriptorを再利用しない。heap全体の破棄はGPU完了後。
	class DescriptorAllocator : private KT::Core::NonCopyable
	{
	public:
		explicit DescriptorAllocator(DescriptorHeap& heap);
		DescriptorHandle Allocate();
	private:
		DescriptorHeap& heap_;
	};
}
