#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Graphics/GraphicsDevice.h>
#include <cstdint>
#include <array>

namespace KT::Graphics
{
	// 前方宣言
	class  CommandQueue;

	// Swapchainの所有と状態の取得
	class Swapchain : private KT::Core::NonCopyable
	{
	public:
		// コンストラクタ(GraphicsDevice, CommandQueue, HWND, 幅, 高さ)
		Swapchain(GraphicsDevice& device, CommandQueue& queue, HWND window, std::uint32_t width, std::uint32_t height);

		// デストラクタ
		~Swapchain() = default;

	private:
		ComPtr<IDXGISwapChain3> swapchain_;
	public:
		// バックバッファの数
		static constexpr std::uint32_t BufferCount = 2;

	private:
		// バックバッファのリソース
		std::array<ComPtr<ID3D12Resource>, BufferCount> backBuffers_{};

		// レンダーターゲットビューのヒープ
		ComPtr<ID3D12DescriptorHeap> rtvHeap_{};

		// レンダーターゲットビューのヒープのサイズ
		UINT rtvDescriptorSize_ = 0;

	public:
		// 現在のバックバッファのインデックスを取得
		UINT GetCurrentBackBufferIndex() const;
		// バックバッファのリソースを取得
		ID3D12Resource* GetBackBuffer(UINT index) const;
		// レンダーターゲットビューのヒープを取得
		D3D12_CPU_DESCRIPTOR_HANDLE GetRtv(UINT index) const;

	public:
		// Present(1, 0)を呼ぶ
		void Present();
	};
}