#include <Graphics/DepthTargetView.h>
#include <Graphics/DepthBuffer.h>
#include <Graphics/GraphicsValidation.h>
#include <stdexcept>

namespace KT::Graphics
{
	// 未実装契約: constructor/getterは未実装。DepthBufferからborrow値を検証して作る。任意raw handleで成功を装わない。
	DepthTargetView::DepthTargetView(const DepthBuffer& owner)
	{
	}

	ID3D12Resource* DepthTargetView::GetResource() const noexcept
	{
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DepthTargetView::GetDsv() const noexcept
	{
	}

	UINT DepthTargetView::GetWidth() const noexcept
	{
	}

	UINT DepthTargetView::GetHeight() const noexcept
	{
	}

	DXGI_FORMAT DepthTargetView::GetFormat() const noexcept
	{
	}

}
