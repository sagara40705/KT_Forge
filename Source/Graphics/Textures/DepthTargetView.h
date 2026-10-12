#pragma once
#include <d3d12.h>

namespace KT::Graphics
{
	class DepthBuffer;

	// DepthBufferの画像・DSV・寸法・formatを借用する、copy可能な値。
	// ownerへのポインタを保持しない。画像とheapは最後の利用Fenceまで外部で保持する。
	// D32_FLOAT/sample1/1Mip/1sliceのDSV。readonly深度は所有元の専用descriptorを借用する。
	class DepthTargetView
	{
	public:
		explicit DepthTargetView(const DepthBuffer& owner, bool readOnly = false);

		// descriptorの書込可否。Graphのusageと実描画のPSOへ照合する。
		bool IsReadOnly() const noexcept
		{
			return readOnly_;
		}

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
		bool readOnly_ = false;
	};
}
