#include <Graphics/Texture.h>
#include <Graphics/GraphicsValidation.h>
#include <stdexcept>

namespace KT::Graphics
{
	Texture::Texture(GraphicsDevice& device, UINT width, UINT height, DXGI_FORMAT format, TextureUsage usage)
		: width_(width),height_(height),format_(format)
	{
		if (width==0 || height==0 || width>D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || height>D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
			throw std::invalid_argument("Texture size is out of range.");
		if ((usage!=TextureUsage::RenderTarget && usage!=TextureUsage::Depth) ||
			(usage==TextureUsage::RenderTarget && !IsColorTargetFormat(format)) || (usage==TextureUsage::Depth && format!=DXGI_FORMAT_D32_FLOAT))
			throw std::invalid_argument("Texture usage/format is unsupported.");
		D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format};
		CheckGraphicsResult(device.GetDevice()->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support)),"Format support query failed.");
		const auto required=usage==TextureUsage::Depth?D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL:D3D12_FORMAT_SUPPORT1_RENDER_TARGET;
		if ((support.Support1&required)==0) throw std::invalid_argument("Texture format is unsupported by device.");
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D; desc.Width=width; desc.Height=height;
		desc.DepthOrArraySize=1; desc.MipLevels=1; desc.Format=format; desc.SampleDesc.Count=1;
		desc.Flags=usage==TextureUsage::Depth?D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL:D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type=D3D12_HEAP_TYPE_DEFAULT; heap.CreationNodeMask=heap.VisibleNodeMask=1;
		D3D12_CLEAR_VALUE clear{};
		clear.Format=format;
		if (usage==TextureUsage::RenderTarget) clear.Color[3]=1; // black/alpha1。
		else clear.DepthStencil.Depth=0; // 有限far Reverse-Z。
		CheckGraphicsResult(device.GetDevice()->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,
			D3D12_RESOURCE_STATE_COMMON,&clear,IID_PPV_ARGS(resource_.GetAddressOf())),"Texture creation failed.");
	}
}
