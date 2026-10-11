#pragma once
#include <Graphics/Descriptors/DescriptorHeap.h>

namespace KT::Graphics
{
	// 不変Samplerとshader-visible heapを所有する。最後の利用Fenceまで保持する。
	class Sampler : private KT::Core::NonCopyable
	{
	public:
		Sampler(GraphicsDevice& device, D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE addressU, D3D12_TEXTURE_ADDRESS_MODE addressV);

		[[nodiscard]] ID3D12DescriptorHeap* GetHeap() const noexcept
		{
			return heap_.GetHeap();
		}

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetSampler() const
		{
			return heap_.GetGpu(sampler_);
		}

	private:
		DescriptorHeap heap_;
		DescriptorHandle sampler_;
	};
}
