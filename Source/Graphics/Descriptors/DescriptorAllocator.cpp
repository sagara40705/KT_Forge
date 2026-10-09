#include <Graphics/Descriptors/DescriptorAllocator.h>
#include <stdexcept>

namespace KT::Graphics
{
	DescriptorAllocator::DescriptorAllocator(DescriptorHeap& heap)
		: heap_(heap)
	{
		// 同じheapに複数のallocatorを作らない
		if (heap.allocatorClaimed_)
		{
			throw std::logic_error("Descriptor heap already has an allocator.");
		}
		heap.allocatorClaimed_ = true;
	}

	DescriptorHandle DescriptorAllocator::Allocate()
	{
		return heap_.Allocate();
	}
}
