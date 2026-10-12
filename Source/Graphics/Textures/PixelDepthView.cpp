#include <Graphics/Textures/PixelDepthView.h>
#include <stdexcept>

namespace KT::Graphics
{
	PixelDepthView::PixelDepthView(const DepthBuffer& owner)
		: resource_(owner.GetTexture().GetResource()),
		  heap_(owner.GetSrvHeap()),
		  srv_(owner.GetSrv())
	{
		if (!resource_ || !heap_ || srv_.ptr == 0 || resource_->GetDesc().Format != DXGI_FORMAT_R32_TYPELESS)
		{
			throw std::invalid_argument("PixelDepthViewには読取対応のR32_TYPELESS深度画像とSRVが必要です。");
		}
	}
}
