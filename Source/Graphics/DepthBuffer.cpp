#include <Graphics/DepthBuffer.h>

namespace KT::Graphics
{
	DepthBuffer::DepthBuffer(GraphicsDevice& device, UINT width, UINT height, DXGI_FORMAT format)
		: texture_(device,width,height,format,TextureUsage::Depth),
		  heap_(device,D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1),allocator_(heap_),handle_(allocator_.Allocate())
	{
		device.GetDevice()->CreateDepthStencilView(texture_.GetResource(),nullptr,GetDsv());
	}
}
