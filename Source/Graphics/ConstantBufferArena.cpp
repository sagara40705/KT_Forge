#include <Graphics/ConstantBufferArena.h>
#include <Graphics/GraphicsValidation.h>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace KT::Graphics
{
	ConstantBufferArena::ConstantBufferArena(GraphicsDevice& device, std::size_t capacity) : capacity_(capacity)
	{
		if (capacity==0 || capacity%256!=0 || capacity>(std::numeric_limits<UINT>::max)())
			throw std::invalid_argument("Constant arena capacity must be nonzero, 256-aligned and fit UINT.");
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; desc.Width=capacity; desc.Height=1;
		desc.DepthOrArraySize=1; desc.MipLevels=1; desc.SampleDesc.Count=1; desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type=D3D12_HEAP_TYPE_UPLOAD; heap.CreationNodeMask=heap.VisibleNodeMask=1;
		CheckGraphicsResult(device.GetDevice()->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,IID_PPV_ARGS(resource_.GetAddressOf())),"Constant arena creation failed.");
		void* mapped=nullptr;
		const D3D12_RANGE noRead{0,0};
		CheckGraphicsResult(resource_->Map(0,&noRead,&mapped),"Constant arena Map failed.");
		mapped_=static_cast<std::byte*>(mapped);
	}
	ConstantBufferArena::~ConstantBufferArena()
	{
		if (mapped_) resource_->Unmap(0,nullptr);
	}
	void ConstantBufferArena::Begin()
	{
		if (writable_) throw std::logic_error("Constant arena is already writable.");
		if (epoch_==(std::numeric_limits<std::uint64_t>::max)()) throw std::overflow_error("Constant epoch exhausted.");
		++epoch_; cursor_=0; writable_=true;
	}
	ConstantSlice ConstantBufferArena::Write(std::span<const std::byte> bytes)
	{
		if (!writable_) throw std::logic_error("Constant writes require an active frame.");
		if (bytes.empty() || !bytes.data() || bytes.size()>65536) throw std::invalid_argument("Constant payload must be 1..65536 bytes.");
		const auto reserved=(bytes.size()+255)&~std::size_t{255};
		if (reserved>capacity_-cursor_) throw std::overflow_error("Constant arena is full.");
		ConstantSlice result;
		result.owner_=this; result.epoch_=epoch_; result.offset_=cursor_; result.size_=bytes.size();
		std::memset(mapped_+cursor_,0,reserved);
		std::memcpy(mapped_+cursor_,bytes.data(),bytes.size());
		cursor_+=reserved;
		return result;
	}
	D3D12_GPU_VIRTUAL_ADDRESS ConstantBufferArena::GetAddress(ConstantSlice s) const
	{
		if (!writable_ || s.owner_!=this || s.epoch_!=epoch_ || s.size_==0 || s.offset_%256!=0 ||
			s.offset_>cursor_ || s.size_>cursor_-s.offset_)
			throw std::invalid_argument("Constant slice is inactive/foreign/stale/out of range.");
		return resource_->GetGPUVirtualAddress()+s.offset_;
	}
}
