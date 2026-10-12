#pragma once
#include <Graphics/Textures/DepthBuffer.h>

namespace KT::Graphics
{
	// sampleable DepthBufferのR32_FLOAT SRVを値借用する。画像とCPU heapはFenceまで外部所有する。
	class PixelDepthView
	{
	public:
		explicit PixelDepthView(const DepthBuffer& owner);

		ID3D12Resource* GetResource() const noexcept
		{
			return resource_;
		}

		ID3D12DescriptorHeap* GetHeap() const noexcept
		{
			return heap_;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetSrv() const noexcept
		{
			return srv_;
		}

	private:
		// 画像とSRV heapは外部所有者から借用する。
		ID3D12Resource* resource_ = nullptr;
		ID3D12DescriptorHeap* heap_ = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE srv_{};
	};
}
