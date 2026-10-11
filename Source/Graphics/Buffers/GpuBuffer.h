#pragma once
#include <Graphics/GraphicsDevice.h>
#include <cstddef>
#include <span>

namespace KT::Graphics
{
	class CommandContext;
	class CommandQueue;
	// 直接入力の互換経路と、明示転送が必要な静的GPU配置を区別する。
	enum class BufferStorage
	{
		Upload,
		Default
	};

	// 不変Bufferを所有する。DEFAULTはUpload完了後だけ読め、stagingは完了Fence後に解放する。
	class GpuBuffer : private KT::Core::NonCopyable
	{
	public:
		GpuBuffer(GraphicsDevice& device, std::span<const std::byte> bytes, BufferStorage storage = BufferStorage::Upload,
			D3D12_RESOURCE_STATES finalState = D3D12_RESOURCE_STATE_GENERIC_READ);
		void Upload(CommandQueue& queue);

		[[nodiscard]] bool IsReady() const noexcept
		{
			return ready_;
		}

		ID3D12Resource* GetResource() const noexcept
		{
			return resource_.Get();
		}

		std::size_t GetSize() const noexcept
		{
			return size_;
		}

	private:
		// 明示転送Contextの生成元。Bufferより長く保持する。
		GraphicsDevice& device_;
		ComPtr<ID3D12Resource> resource_;
		std::size_t size_ = 0;
		ComPtr<ID3D12Resource> staging_;
		D3D12_RESOURCE_STATES finalState_ = D3D12_RESOURCE_STATE_GENERIC_READ;
		bool ready_ = false;
	};
}
