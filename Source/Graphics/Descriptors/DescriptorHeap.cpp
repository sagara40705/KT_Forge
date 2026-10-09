#include <Graphics/Descriptors/DescriptorHeap.h>
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
			// 再利用しないheapの個体IDを発行する
			auto currentId = nextId.load();
			for (;;)
			{
				if (currentId == (std::numeric_limits<std::uint64_t>::max)())
				{
					throw std::overflow_error("Descriptor heap ID exhausted.");
				}
				if (nextId.compare_exchange_weak(currentId, currentId + 1))
				{
					return currentId;
				}
			}
		}
	}

	DescriptorHeap::DescriptorHeap(GraphicsDevice& device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity, bool shaderVisible)
		: capacity_(capacity),
		  type_(type),
		  visible_(shaderVisible)
	{
		// heap種別・容量・shader可視性を確認する
		if (type != D3D12_DESCRIPTOR_HEAP_TYPE_RTV && type != D3D12_DESCRIPTOR_HEAP_TYPE_DSV &&
			type != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && type != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		{
			throw std::invalid_argument("Descriptor heap type is invalid.");
		}
		if (capacity == 0 || capacity > 1000000 || (type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER && capacity > 2048))
		{
			throw std::invalid_argument("Descriptor heap capacity is out of range.");
		}
		if (shaderVisible && type != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && type != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		{
			throw std::invalid_argument("RTV/DSV heaps cannot be shader visible.");
		}

		// 指定した種別と容量でheapを作成する
		const D3D12_DESCRIPTOR_HEAP_DESC desc{
			type, capacity, shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
		CheckGraphicsResult(
			device.GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(heap_.GetAddressOf())), "Descriptor heap creation failed.");

		// descriptor間隔を確認してheapの個体IDを発行する
		stride_ = device.GetDevice()->GetDescriptorHandleIncrementSize(type);
		if (stride_ == 0)
		{
			throw std::runtime_error("Descriptor stride is zero.");
		}
		id_ = IssueId();
	}

	DescriptorHandle DescriptorHeap::Allocate()
	{
		// 空きslotを確認して非所有tokenを発行する
		if (used_ == capacity_)
		{
			throw std::overflow_error("Descriptor heap is full.");
		}
		DescriptorHandle result;
		result.heapId_ = id_;
		result.index_ = used_++;
		return result;
	}

	void DescriptorHeap::Validate(DescriptorHandle handle) const
	{
		// このheapから発行済みのtokenか確認する
		if (handle.heapId_ != id_ || handle.index_ >= used_)
		{
			throw std::invalid_argument("Descriptor token is foreign/unallocated.");
		}
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCpu(DescriptorHandle handle, D3D12_DESCRIPTOR_HEAP_TYPE expectedType) const
	{
		// tokenの所属とCPUビューの用途を確認する
		Validate(handle);
		if (type_ != expectedType)
		{
			throw std::invalid_argument("Descriptor heap type does not match usage.");
		}

		// アドレスの上限を確認してCPU handleを求める
		auto result = heap_->GetCPUDescriptorHandleForHeapStart();
		const auto offset = std::uint64_t(handle.index_) * stride_;
		if (offset > (std::numeric_limits<SIZE_T>::max)() - result.ptr)
		{
			throw std::overflow_error("CPU descriptor address overflow.");
		}
		result.ptr += static_cast<SIZE_T>(offset);
		return result;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGpu(DescriptorHandle handle) const
	{
		Validate(handle);

		// GPU handleを取得できるheapか確認する
		if (!visible_)
		{
			throw std::logic_error("Descriptor heap is not shader visible.");
		}

		// アドレスの上限を確認してGPU handleを求める
		auto result = heap_->GetGPUDescriptorHandleForHeapStart();
		const auto offset = std::uint64_t(handle.index_) * stride_;
		if (offset > (std::numeric_limits<UINT64>::max)() - result.ptr)
		{
			throw std::overflow_error("GPU descriptor address overflow.");
		}
		result.ptr += offset;
		return result;
	}
}
