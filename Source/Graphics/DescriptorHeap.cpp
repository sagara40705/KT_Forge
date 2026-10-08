#include <Graphics/DescriptorHeap.h>
#include <Graphics/GraphicsValidation.h>
#include <atomic>
#include <limits>
#include <stdexcept>

namespace KT::Graphics
{
	namespace
	{
		std::atomic<std::uint64_t> nextId{1};
		std::uint64_t IssueId()
		{
			auto current=nextId.load();
			for (;;) {
				if (current==(std::numeric_limits<std::uint64_t>::max)()) throw std::overflow_error("Descriptor heap ID exhausted.");
				if (nextId.compare_exchange_weak(current,current+1)) return current;
			}
		}
	}
	DescriptorHeap::DescriptorHeap(GraphicsDevice& device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity, bool visible)
		: capacity_(capacity),type_(type),visible_(visible)
	{
		if (type!=D3D12_DESCRIPTOR_HEAP_TYPE_RTV && type!=D3D12_DESCRIPTOR_HEAP_TYPE_DSV &&
			type!=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && type!=D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
			throw std::invalid_argument("Descriptor heap type is invalid.");
		if (capacity==0 || capacity>1000000 || (type==D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER && capacity>2048))
			throw std::invalid_argument("Descriptor heap capacity is out of range.");
		if (visible && type!=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && type!=D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
			throw std::invalid_argument("RTV/DSV heaps cannot be shader visible.");
		const D3D12_DESCRIPTOR_HEAP_DESC desc{type,capacity,visible?D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE:D3D12_DESCRIPTOR_HEAP_FLAG_NONE,0};
		CheckGraphicsResult(device.GetDevice()->CreateDescriptorHeap(&desc,IID_PPV_ARGS(heap_.GetAddressOf())),"Descriptor heap creation failed.");
		stride_=device.GetDevice()->GetDescriptorHandleIncrementSize(type);
		if (stride_==0) throw std::runtime_error("Descriptor stride is zero.");
		id_=IssueId();
	}
	DescriptorHandle DescriptorHeap::Allocate()
	{
		if (used_==capacity_) throw std::overflow_error("Descriptor heap is full.");
		DescriptorHandle result;
		result.heapId_=id_; result.index_=used_++;
		return result;
	}
	void DescriptorHeap::Validate(DescriptorHandle h) const
	{
		if (h.heapId_!=id_ || h.index_>=used_) throw std::invalid_argument("Descriptor token is foreign/unallocated.");
	}
	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCpu(DescriptorHandle h, D3D12_DESCRIPTOR_HEAP_TYPE expected) const
	{
		Validate(h);
		if (type_!=expected) throw std::invalid_argument("Descriptor heap type does not match usage.");
		auto result=heap_->GetCPUDescriptorHandleForHeapStart();
		const auto offset=std::uint64_t(h.index_)*stride_;
		if (offset>(std::numeric_limits<SIZE_T>::max)()-result.ptr) throw std::overflow_error("CPU descriptor address overflow.");
		result.ptr+=static_cast<SIZE_T>(offset);
		return result;
	}
	D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGpu(DescriptorHandle h) const
	{
		Validate(h);
		if (!visible_) throw std::logic_error("Descriptor heap is not shader visible.");
		auto result=heap_->GetGPUDescriptorHandleForHeapStart();
		const auto offset=std::uint64_t(h.index_)*stride_;
		if (offset>(std::numeric_limits<UINT64>::max)()-result.ptr) throw std::overflow_error("GPU descriptor address overflow.");
		result.ptr+=offset;
		return result;
	}
}
