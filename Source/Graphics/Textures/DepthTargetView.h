#pragma once
#include <d3d12.h>

namespace KT::Graphics
{
	class DepthBuffer;

	// DepthBufferの画像・DSV・寸法・formatを借用する、copy可能な値。
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
