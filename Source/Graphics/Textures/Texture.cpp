#include <Graphics/Textures/Texture.h>
#include <Graphics/GraphicsValidation.h>
#include <stdexcept>

namespace KT::Graphics
{
	Texture::Texture(GraphicsDevice& device, UINT width, UINT height, DXGI_FORMAT format, TextureUsage usage)
		: width_(width),
		  height_(height),
		  format_(format)
	{
		// 画像の寸法・用途・formatを確認する
		if (width == 0 || height == 0 || width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
		{
			throw std::invalid_argument("Texture size is out of range.");
		}
		if ((usage != TextureUsage::RenderTarget && usage != TextureUsage::Depth) ||
			(usage == TextureUsage::RenderTarget && !IsColorTargetFormat(format)) ||
			(usage == TextureUsage::Depth && format != DXGI_FORMAT_D32_FLOAT))
		{
			throw std::invalid_argument("Texture usage/format is unsupported.");
		}

		// Deviceが指定した描画用途に対応するか確認する
		D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport{format};
		CheckGraphicsResult(device.GetDevice()->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport)),
			"Format support query failed.");
		const auto requiredSupport =
			usage == TextureUsage::Depth ? D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL : D3D12_FORMAT_SUPPORT1_RENDER_TARGET;
		if ((formatSupport.Support1 & requiredSupport) == 0)
		{
			throw std::invalid_argument("Texture format is unsupported by device.");
		}

		// 2D・1mip・1slice・sample1の画像を設定する
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = width;
		resourceDesc.Height = height;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = format;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags =
			usage == TextureUsage::Depth ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		// DEFAULTヒープに画像を配置する
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CreationNodeMask = heapProperties.VisibleNodeMask = 1;

		// 用途に合わせた最適化Clear値を設定する
		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = format;
		if (usage == TextureUsage::RenderTarget)
		{
			// 色は黒・alpha1を使う
			clearValue.Color[3] = 1;
		}
		else
		{
			// 有限farのReverse-Zに合わせて深度0を使う
			clearValue.DepthStencil.Depth = 0;
		}

		// COMMON状態で画像を作成する
		CheckGraphicsResult(device.GetDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
								D3D12_RESOURCE_STATE_COMMON, &clearValue, IID_PPV_ARGS(resource_.GetAddressOf())),
			"Texture creation failed.");
	}
}
