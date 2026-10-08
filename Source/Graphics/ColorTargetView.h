#pragma once
#include <Graphics/RenderTarget.h>
#include <Graphics/Swapchain.h>

namespace KT::Graphics
{
	// 検証済み所有者から作る非所有のRTV/画像借用。任意raw handleの組は受け取らない。
	// RenderTargetまたはSwapchain/backbufferをGPU完了まで保持する。state追跡/barrierは別責務。
	class ColorTargetView
	{
	public:
		explicit ColorTargetView(const RenderTarget& target);
		ColorTargetView(const Swapchain& swapchain, UINT index);
		ID3D12Resource* GetResource() const noexcept { return resource_; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetRtv() const noexcept { return rtv_; }
		UINT GetWidth() const noexcept { return width_; }
		UINT GetHeight() const noexcept { return height_; }
		DXGI_FORMAT GetFormat() const noexcept { return format_; }
	private:
		void Inspect();
		ID3D12Resource* resource_=nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE rtv_{};
		UINT width_=0,height_=0;
		DXGI_FORMAT format_=DXGI_FORMAT_UNKNOWN;
	};
}
