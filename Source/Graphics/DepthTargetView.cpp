#include <Graphics/DepthTargetView.h>
#include <Graphics/DepthBuffer.h>
#include <Graphics/GraphicsValidation.h>
#include <stdexcept>

namespace KT::Graphics
{
	// TODO: 未実装。DepthBufferの画像・DSV・寸法・formatを検査し、借用値として保持する。
	DepthTargetView::DepthTargetView(const DepthBuffer& owner)
	{
	}

	// TODO: 未実装。保持した借用値を返す。
	ID3D12Resource* DepthTargetView::GetResource() const noexcept
	{
	}

	// TODO: 未実装。保持した借用値を返す。
	D3D12_CPU_DESCRIPTOR_HANDLE DepthTargetView::GetDsv() const noexcept
	{
	}

	// TODO: 未実装。保持した借用値を返す。
	UINT DepthTargetView::GetWidth() const noexcept
	{
	}

	// TODO: 未実装。保持した借用値を返す。
	UINT DepthTargetView::GetHeight() const noexcept
	{
	}

	// TODO: 未実装。保持した借用値を返す。
	DXGI_FORMAT DepthTargetView::GetFormat() const noexcept
	{
	}

}
