#pragma once
#include <Graphics/Descriptors/DescriptorHeap.h>
#include <span>
#include <vector>

namespace KT::Graphics
{
	class CommandQueue;

	// 行間paddingのないRGBA8。入力はconstructorが戻るまで保持する。
	struct TextureMipData
	{
		UINT width, height;
		std::span<const std::uint8_t> pixels;
	};

	// 不変2D画像とSRVを所有する。明示Upload後はPixel Shaderの読取専用。
	// Graphの書込画像として貸し出さない。最後の描画Fence完了まで所有者が保持する。
	class SampledTexture : private KT::Core::NonCopyable
	{
	public:
		SampledTexture(GraphicsDevice& device, std::span<const TextureMipData> mips, DXGI_FORMAT format);
		// COPY_DESTからPIXEL_SHADER_RESOURCEへ遷移し、転送完了を待ってstagingを解放する。
		void Upload(CommandQueue& queue);

		[[nodiscard]] bool IsReady() const noexcept
		{
			return ready_;
		}

		[[nodiscard]] ID3D12Resource* GetResource() const noexcept
		{
			return resource_.Get();
		}

		[[nodiscard]] ID3D12DescriptorHeap* GetHeap() const noexcept
		{
			return heap_.GetHeap();
		}

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetSrv() const;

	private:
		GraphicsDevice& device_;
		DescriptorHeap heap_;
		DescriptorHandle srv_;
		ComPtr<ID3D12Resource> resource_, staging_;
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints_;
		bool ready_ = false;
	};
}
