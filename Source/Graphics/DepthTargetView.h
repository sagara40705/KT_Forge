#pragma once
#include <d3d12.h>

namespace KT::Graphics
{
	class DepthBuffer;
	// DepthBufferから検証して作る非所有借用値。画像/DSV/formatを値として保持し、copy可能。
	// ownerへのポインタを保持しない。画像とheapは最後の利用Fenceまで外部で保持する。
	// 初版はD32_FLOAT/sample1/1Mip/1sliceの書込可能DSV。readonly/stencil/任意raw組は未対応。
	class DepthTargetView
	{
	public:
		explicit DepthTargetView(const DepthBuffer& owner);
		ID3D12Resource* GetResource() const noexcept;
		D3D12_CPU_DESCRIPTOR_HANDLE GetDsv() const noexcept;
		UINT GetWidth() const noexcept;
		UINT GetHeight() const noexcept;
		DXGI_FORMAT GetFormat() const noexcept;
	private:
		ID3D12Resource* resource_ = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE dsv_{};
		UINT width_ = 0, height_ = 0;
		DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
	};
}
