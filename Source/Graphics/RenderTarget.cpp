#include <Graphics/RenderTarget.h>

namespace KT::Graphics
{
	RenderTarget::RenderTarget(GraphicsDevice& device, UINT width, UINT height, DXGI_FORMAT format)
		: texture_(device,width,height,format,TextureUsage::RenderTarget),
		  heap_(device,D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1),allocator_(heap_),handle_(allocator_.Allocate())
	{
		device.GetDevice()->CreateRenderTargetView(texture_.GetResource(),nullptr,GetRtv());
	}
}
