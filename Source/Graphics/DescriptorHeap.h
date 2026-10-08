#pragma once
#include <Graphics/GraphicsDevice.h>
#include <Graphics/DescriptorHandle.h>

namespace KT::Graphics
{
	class DescriptorAllocator;
	// descriptorだけを所有。画像は所有しない。CPU/GPU handleはheapの借用値。
	class DescriptorHeap : private KT::Core::NonCopyable
	{
	public:
		DescriptorHeap(GraphicsDevice& device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity, bool shaderVisible=false);
		D3D12_CPU_DESCRIPTOR_HANDLE GetCpu(DescriptorHandle handle, D3D12_DESCRIPTOR_HEAP_TYPE expectedType) const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetGpu(DescriptorHandle handle) const;
		ID3D12DescriptorHeap* GetHeap() const noexcept { return heap_.Get(); }
		UINT GetStride() const noexcept { return stride_; }
		D3D12_DESCRIPTOR_HEAP_TYPE GetType() const noexcept { return type_; }
	private:
		friend class DescriptorAllocator;
		DescriptorHandle Allocate();
		void Validate(DescriptorHandle handle) const;
		ComPtr<ID3D12DescriptorHeap> heap_;
		std::uint64_t id_=0;
		UINT stride_=0, capacity_=0, used_=0;
		D3D12_DESCRIPTOR_HEAP_TYPE type_;
		bool visible_=false, allocatorClaimed_=false;
	};
}
