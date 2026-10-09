#include <Graphics/Presentation/Swapchain.h>
#include <Graphics/Commands/CommandQueue.h>
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
		DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
		swapchainDesc.Width = width;
		swapchainDesc.Height = height;
		swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchainDesc.Stereo = FALSE;
		swapchainDesc.SampleDesc.Count = 1;
		swapchainDesc.SampleDesc.Quality = 0;
		swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainDesc.BufferCount = BufferCount;
		swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		swapchainDesc.Flags = 0;

		ComPtr<IDXGISwapChain1> createdSwapchain;
		if (FAILED(device.GetFactory()->CreateSwapChainForHwnd(
				queue.GetCommandQueue(), window, &swapchainDesc, nullptr, nullptr, createdSwapchain.GetAddressOf())))
		{
			throw std::runtime_error("Swapchainの作成に失敗しました。");
		}

		// IDXGISwapChain3のインターフェースを取得する
		if (FAILED(createdSwapchain.As(&swapchain_)))
		{
			throw std::runtime_error("SwapChain3のインターフェースの取得に失敗しました。");
		}

		// ウィンドウの関連付けを行う（Alt+Enterによるフルスクリーン切り替えを無効化）
		if (FAILED(device.GetFactory()->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER)))
		{
			throw std::runtime_error("ウィンドウの関連付けに失敗しました。");
		}

		// バックバッファの取得
		for (std::uint32_t index = 0; index < BufferCount; ++index)
		{
			if (FAILED(swapchain_->GetBuffer(index, IID_PPV_ARGS(&backBuffers_[index]))))
			{
				throw std::runtime_error("バックバッファの取得に失敗しました。");
			}
		}

		// レンダーターゲットビューのヒープの作成
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.NumDescriptors = BufferCount;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		rtvHeapDesc.NodeMask = 0;
		if (FAILED(device.GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_))))
		{
			throw std::runtime_error("レンダーターゲットビューのヒープの作成に失敗しました。");
		}

		// RTVの間隔を取得
		rtvDescriptorSize_ = device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// バックバッファに対してレンダーターゲットビューを作成
		auto rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
		for (std::uint32_t index = 0; index < BufferCount; ++index)
		{
			device.GetDevice()->CreateRenderTargetView(backBuffers_[index].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += rtvDescriptorSize_;
		}
	}

	UINT Swapchain::GetCurrentBackBufferIndex() const
	{
		return swapchain_->GetCurrentBackBufferIndex();
	}

	ID3D12Resource* Swapchain::GetBackBuffer(UINT index) const
	{
		if (index >= BufferCount)
		{
			throw std::out_of_range("バックバッファのインデックスが範囲外です。");
		}
		return backBuffers_[index].Get();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Swapchain::GetRtv(UINT index) const
	{
		if (index >= BufferCount)
		{
			throw std::out_of_range("レンダーターゲットビューのインデックスが範囲外です。");
		}
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
		rtvHandle.ptr += static_cast<SIZE_T>(index) * rtvDescriptorSize_;
		return rtvHandle;
	}

	void Swapchain::Present()
	{
		if (FAILED(swapchain_->Present(1, 0)))
		{
			throw std::runtime_error("SwapchainのPresentに失敗しました。");
		}
	}
}