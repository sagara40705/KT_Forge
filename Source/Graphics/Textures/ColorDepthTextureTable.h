#pragma once
#include <Graphics/Textures/PixelDepthView.h>
#include <Graphics/Textures/SampledTexture.h>

namespace KT::Graphics
{
	// t0色・t1影を同じshader-visible heapへコピーして所有する。借用画像はFenceまで外部保持する。
	class ColorDepthTextureTable : private KT::Core::NonCopyable
	{
	public:
		ColorDepthTextureTable(GraphicsDevice& device, const SampledTexture* color, PixelDepthView depth);

		ID3D12DescriptorHeap* GetHeap() const noexcept
		{
			return heap_.GetHeap();
		}

		D3D12_GPU_DESCRIPTOR_HANDLE GetTable() const
		{
			return heap_.GetGpu(first_);
		}

		ID3D12Resource* GetDepthResource() const noexcept
		{
			return depth_;
		}

		const SampledTexture* GetColor() const noexcept
		{
			return color_;
		}

	private:
		DescriptorHeap heap_;
		DescriptorHandle first_;
		// SRVのコピーは画像所有権を延長しない。
		const SampledTexture* color_ = nullptr;
		ID3D12Resource* depth_ = nullptr;
	};
}
