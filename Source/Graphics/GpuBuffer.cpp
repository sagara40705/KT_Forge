#include <Graphics/GpuBuffer.h>
#include <Graphics/GraphicsValidation.h>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace KT::Graphics
{
	GpuBuffer::GpuBuffer(GraphicsDevice& device, std::span<const std::byte> bytes)
	{
		if (bytes.empty() || !bytes.data()) throw std::invalid_argument("GpuBuffer input is empty.");
		if (bytes.size()>(std::numeric_limits<UINT>::max)()) throw std::overflow_error("GpuBuffer exceeds UINT view size.");
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type=D3D12_HEAP_TYPE_UPLOAD; heap.CreationNodeMask=heap.VisibleNodeMask=1;
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; desc.Width=bytes.size(); desc.Height=1;
		desc.DepthOrArraySize=1; desc.MipLevels=1; desc.SampleDesc.Count=1; desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		CheckGraphicsResult(device.GetDevice()->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(resource_.GetAddressOf())),"GpuBuffer creation failed.");
		void* mapped=nullptr;
		const D3D12_RANGE noRead{0,0};
		CheckGraphicsResult(resource_->Map(0,&noRead,&mapped),"GpuBuffer Map failed.");
		std::memcpy(mapped,bytes.data(),bytes.size());
		const D3D12_RANGE written{0,bytes.size()};
		resource_->Unmap(0,&written);
		size_=bytes.size();
	}
}
