#pragma once
#include <Graphics/Textures/Texture.h>
#include <Graphics/Descriptors/DescriptorAllocator.h>

namespace KT::Graphics
{
	// D32_FLOAT画像と専用DSV heapを所有。Reverse-Z clear0、PSO比較GREATER。
	class DepthBuffer : private KT::Core::NonCopyable
	{
	public:
		DepthBuffer(GraphicsDevice& device, UINT width, UINT height, DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT);

		const Texture& GetTexture() const noexcept
		{
			return texture_;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetDsv() const
		{
			return heap_.GetCpu(handle_, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		}

	private:
		Texture texture_;
		DescriptorHeap heap_;
		DescriptorAllocator allocator_;
		DescriptorHandle handle_;
	};
}
