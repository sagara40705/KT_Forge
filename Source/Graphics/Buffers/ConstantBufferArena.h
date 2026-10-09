#pragma once
#include <Graphics/GraphicsDevice.h>
#include <cstddef>
#include <cstdint>
#include <span>

namespace KT::Graphics
{
	class ConstantBufferArena;
	class FrameResources;

	// 非所有の割当token。owner/再利用しないarena個体ID/epoch/範囲を検査する。
	class ConstantSlice
	{
	public:
		ConstantSlice() = default;

	private:
		friend class ConstantBufferArena;
		const ConstantBufferArena* owner_ = nullptr;
		std::uint64_t arenaId_ = 0;
		std::uint64_t epoch_ = 0;
		std::size_t offset_ = 0, size_ = 0;
	};

	struct RootConstantBinding
	{
		UINT rootParameter = 0;
		ConstantSlice slice;
	};

	// persistently mapped UPLOADを所有。各Writeは新しい256byte整列領域、既存領域の上書きAPIなし。
	// Begin/SealはFrameResourcesだけが行う。単一thread、GPU完了前のresetは禁止。
	class ConstantBufferArena : private KT::Core::NonCopyable
	{
	public:
		ConstantBufferArena(GraphicsDevice& device, std::size_t capacity);
		~ConstantBufferArena();
		ConstantSlice Write(std::span<const std::byte> bytes);
		D3D12_GPU_VIRTUAL_ADDRESS GetAddress(ConstantSlice slice) const;

		ID3D12Resource* GetResource() const noexcept
		{
			return resource_.Get();
		}

		std::size_t GetUsedBytes() const noexcept
		{
			return cursor_;
		}

	private:
		friend class FrameResources;
		void Begin();

		void Seal() noexcept
		{
			writable_ = false;
		}

		ComPtr<ID3D12Resource> resource_;
		std::byte* mapped_ = nullptr;
		std::size_t capacity_ = 0, cursor_ = 0;
		// 再構築時も別ID。BeginはこのIDを変えずepochだけを進める。0は未発行。
		std::uint64_t arenaId_ = 0;
		std::uint64_t epoch_ = 0;
		bool writable_ = false;
	};
}
