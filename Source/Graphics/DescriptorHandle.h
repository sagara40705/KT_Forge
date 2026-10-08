#pragma once
#include <cstdint>

namespace KT::Graphics
{
	class DescriptorHeap;
	// 非所有token。heap破棄後は使わない。別heap/未発行tokenの検査に使う。
	class DescriptorHandle
	{
	public:
		DescriptorHandle()=default;
	private:
		friend class DescriptorHeap;
		std::uint64_t heapId_=0;
		std::uint32_t index_=0;
	};
}
