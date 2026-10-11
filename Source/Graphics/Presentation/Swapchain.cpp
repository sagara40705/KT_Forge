#include <Graphics/Presentation/Swapchain.h>
#include <Graphics/Commands/CommandQueue.h>
#include <Graphics/GraphicsValidation.h>
#include <stdexcept>

namespace KT::Graphics
{
	Swapchain::Swapchain(GraphicsDevice& device, CommandQueue& queue, HWND window, std::uint32_t width, std::uint32_t height, bool srgb)
		: device_(device),
		  queue_(queue),
		  rtvFormat_(srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		if (!window)
		{
			throw std::invalid_argument("無効なウィンドウハンドルです。");
		}
		if (width == 0 || height == 0 || width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
		{
			throw std::invalid_argument("Swapchainの幅と高さは1からD3D12の画像上限までが必要です。");
		}
		RequireSameDevice(queue.GetCommandQueue(), device.GetDevice());

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

		AcquireBuffers();
	}

	void Swapchain::AcquireBuffers()
	{
		// 全画像を取得してから公開し、途中失敗で一部だけを使用可能にしない。
		std::array<ComPtr<ID3D12Resource>, BufferCount> buffers;
		for (std::uint32_t index = 0; index < BufferCount; ++index)
		{
			CheckGraphicsResult(swapchain_->GetBuffer(index, IID_PPV_ARGS(&buffers[index])), "BackBufferの取得に失敗しました。");
		}
		auto rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
		for (std::uint32_t index = 0; index < BufferCount; ++index)
		{
			D3D12_RENDER_TARGET_VIEW_DESC view{};
			view.Format = rtvFormat_;
			view.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			device_.GetDevice()->CreateRenderTargetView(buffers[index].Get(), &view, rtvHandle);
			rtvHandle.ptr += rtvDescriptorSize_;
		}
		const auto desc = buffers.front()->GetDesc();
		backBuffers_.swap(buffers);
		width_ = static_cast<UINT>(desc.Width);
		height_ = desc.Height;
	}

	void Swapchain::Resize(std::uint32_t width, std::uint32_t height)
	{
		if (width == 0 || height == 0 || width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
		{
			throw std::invalid_argument("SwapchainのResize寸法がD3D12の画像範囲外です。");
		}
		if (width == width_ && height == height_)
		{
			return;
		}
		// Presentも含めてQueueの最後まで待ち、所有する全BackBuffer参照を解放する。
		queue_.Wait(queue_.Signal());
		width_ = height_ = 0;
		for (auto& buffer : backBuffers_)
		{
			buffer.Reset();
		}
		CheckGraphicsResult(swapchain_->ResizeBuffers(BufferCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0),
			"SwapchainのResizeBuffersに失敗しました。旧BackBufferの借用が残っていないか確認してください。");
		AcquireBuffers();
	}

	void Swapchain::RequireBuffers() const
	{
		if (width_ == 0 || height_ == 0)
		{
			throw std::logic_error("SwapchainのBackBufferが利用できません。Resizeの成功が必要です。");
		}
	}

	UINT Swapchain::GetWidth() const noexcept
	{
		return width_;
	}

	UINT Swapchain::GetHeight() const noexcept
	{
		return height_;
	}

	UINT Swapchain::GetCurrentBackBufferIndex() const
	{
		RequireBuffers();
		return swapchain_->GetCurrentBackBufferIndex();
	}

	ID3D12Resource* Swapchain::GetBackBuffer(UINT index) const
	{
		RequireBuffers();
		if (index >= BufferCount)
		{
			throw std::out_of_range("バックバッファのインデックスが範囲外です。");
		}
		return backBuffers_[index].Get();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Swapchain::GetRtv(UINT index) const
	{
		RequireBuffers();
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
		RequireBuffers();
		if (FAILED(swapchain_->Present(1, 0)))
		{
			throw std::runtime_error("SwapchainのPresentに失敗しました。");
		}
	}
}
