#include <Graphics/Textures/ColorTargetView.h>
#include <Graphics/GraphicsValidation.h>
#include <stdexcept>

namespace KT::Graphics
{
	ColorTargetView::ColorTargetView(const RenderTarget& target)
		: resource_(target.GetTexture().GetResource()),
		  rtv_(target.GetRtv())
	{
		Inspect();
	}

	ColorTargetView::ColorTargetView(const Swapchain& swapchain, UINT index)
		: resource_(swapchain.GetBackBuffer(index)),
		  rtv_(swapchain.GetRtv(index))
	{
		Inspect();
	}

	void ColorTargetView::Inspect()
	{
		// 借用した画像とRTVを確認する
		if (!resource_ || rtv_.ptr == 0)
		{
			throw std::invalid_argument("Color target is null.");
		}

		// 描画先として使える画像の寸法・format・用途を確認する
		const auto resourceDesc = resource_->GetDesc();
		if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || resourceDesc.Width == 0 || resourceDesc.Height == 0 ||
			resourceDesc.Width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || resourceDesc.Height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
			resourceDesc.SampleDesc.Count != 1 || resourceDesc.MipLevels != 1 || resourceDesc.DepthOrArraySize != 1 ||
			!IsColorTargetFormat(resourceDesc.Format) || (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == 0)
		{
			throw std::invalid_argument("Color target description is unsupported.");
		}

		// 検査した寸法とformatを借用値として保持する
		width_ = static_cast<UINT>(resourceDesc.Width);
		height_ = resourceDesc.Height;
		format_ = resourceDesc.Format;
	}
}
