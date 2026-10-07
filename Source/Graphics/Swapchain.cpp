#include <Graphics/Swapchain.h>
#include <Graphics/CommandQueue.h>
#include <stdexcept>

namespace KT::Graphics
{
	Swapchain::Swapchain(GraphicsDevice& device, CommandQueue& queue, HWND window, std::uint32_t width, std::uint32_t height)
	{
		if (!window)
		{
			throw std::invalid_argument("無効なウィンドウハンドルです。");
		}
		if (width == 0 || height == 0)
		{
			throw std::invalid_argument("Swapchainの幅と高さは0以上である必要があります。");
		}

		// Swapchainの作成
		DXGI_SWAP_CHAIN_DESC1 desc{};
		desc.Width = width;
		desc.Height = height;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Stereo = FALSE;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = BufferCount;
		desc.Scaling = DXGI_SCALING_STRETCH;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		desc.Flags = 0;

		ComPtr<IDXGISwapChain1> createdSwapchain;
		if (FAILED(device.GetFactory()->CreateSwapChainForHwnd(queue.GetCommandQueue(), window, &desc, nullptr, nullptr, createdSwapchain.GetAddressOf())))
		{
			throw std::runtime_error("Swapchainの作成に失敗しました。");
		}

		// IDXGISwapChain3のインターフェースを取得する 
		if (FAILED(createdSwapchain.As(&swapchain_)))
		{
			throw std::runtime_error("SwapChain3のインターフェースの取得に失敗しました。");
		}
	}
}