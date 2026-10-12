#include <Graphics/Textures/DepthBuffer.h>
#include <stdexcept>

namespace KT::Graphics
{
	DepthBuffer::DepthBuffer(GraphicsDevice& device, UINT width, UINT height, DXGI_FORMAT format, bool shaderReadable)
		: texture_(device, width, height, format, TextureUsage::Depth, shaderReadable),
		  heap_(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 2),
		  allocator_(heap_),
		  handle_(allocator_.Allocate()),
		  readOnlyHandle_(allocator_.Allocate())
	{
		// 所有する画像を専用heapのDSVへ関連付ける
		D3D12_DEPTH_STENCIL_VIEW_DESC depthView{};
		depthView.Format = format;
		depthView.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		device.GetDevice()->CreateDepthStencilView(texture_.GetResource(), &depthView, GetDsv());
		// Effect passは同じ画像を読み取り、Opaqueが作った深度を保持する。
		depthView.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
		device.GetDevice()->CreateDepthStencilView(texture_.GetResource(), &depthView, GetReadOnlyDsv());
		if (shaderReadable)
		{
			srvHeap_ = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
			DescriptorAllocator allocator(*srvHeap_);
			srvHandle_ = allocator.Allocate();
			D3D12_SHADER_RESOURCE_VIEW_DESC view{};
			view.Format = DXGI_FORMAT_R32_FLOAT;
			view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			view.Texture2D.MipLevels = 1;
			device.GetDevice()->CreateShaderResourceView(texture_.GetResource(), &view, GetSrv());
		}
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DepthBuffer::GetSrv() const
	{
		if (!srvHeap_)
		{
			throw std::logic_error("このDepthBufferはShader読取対応で生成されていません。");
		}
		return srvHeap_->GetCpu(srvHandle_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}
