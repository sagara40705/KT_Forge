#include <Graphics/ColorTargetView.h>
#include <Graphics/GraphicsValidation.h>
#include <stdexcept>

namespace KT::Graphics
{
	ColorTargetView::ColorTargetView(const RenderTarget& target)
		: resource_(target.GetTexture().GetResource()),rtv_(target.GetRtv()) { Inspect(); }
	ColorTargetView::ColorTargetView(const Swapchain& swapchain, UINT index)
		: resource_(swapchain.GetBackBuffer(index)),rtv_(swapchain.GetRtv(index)) { Inspect(); }
	void ColorTargetView::Inspect()
	{
		if (!resource_ || rtv_.ptr==0) throw std::invalid_argument("Color target is null.");
		const auto desc=resource_->GetDesc();
		if (desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.Width==0 || desc.Height==0 ||
			desc.Width>D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || desc.Height>D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
			desc.SampleDesc.Count!=1 || desc.MipLevels!=1 || desc.DepthOrArraySize!=1 || !IsColorTargetFormat(desc.Format) ||
			(desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)==0)
			throw std::invalid_argument("Color target description is unsupported.");
		width_=static_cast<UINT>(desc.Width); height_=desc.Height; format_=desc.Format;
	}
}
