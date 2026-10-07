#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Graphics/GraphicsDevice.h>
#include <cstdint>

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
	};
}