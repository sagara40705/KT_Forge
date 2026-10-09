#pragma once
#include <Graphics/Textures/Texture.h>
#include <Graphics/Descriptors/DescriptorAllocator.h>

namespace KT::Graphics
{
	// 画像と専用1slot RTV heapを共に所有。Swapchainとは別のoffscreen部品。
	class RenderTarget : private KT::Core::NonCopyable
	{
	public:
		RenderTarget(GraphicsDevice& device, UINT width, UINT height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

		const Texture& GetTexture() const noexcept
		{
			return texture_;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetRtv() const
		{
			return heap_.GetCpu(handle_, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

	private:
		Texture texture_;
		DescriptorHeap heap_;
		DescriptorAllocator allocator_;
		DescriptorHandle handle_;
	};
}
