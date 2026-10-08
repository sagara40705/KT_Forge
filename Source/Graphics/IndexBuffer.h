#pragma once
#include <Graphics/GpuBuffer.h>
#include <cstdint>

namespace KT::Graphics
{
	class IndexBuffer : private KT::Core::NonCopyable
	{
	public:
		IndexBuffer(GraphicsDevice& device, std::span<const std::byte> bytes, DXGI_FORMAT format);
		IndexBuffer(GraphicsDevice& device, std::span<const std::uint16_t> values)
			: IndexBuffer(device,std::as_bytes(values),DXGI_FORMAT_R16_UINT) {}
		IndexBuffer(GraphicsDevice& device, std::span<const std::uint32_t> values)
			: IndexBuffer(device,std::as_bytes(values),DXGI_FORMAT_R32_UINT) {}
		const D3D12_INDEX_BUFFER_VIEW& GetView() const noexcept { return view_; }
		UINT GetCount() const noexcept { return count_; }
		UINT GetMaximumIndex() const noexcept { return maximum_; }
		ID3D12Resource* GetResource() const noexcept { return buffer_.GetResource(); }
	private:
		GpuBuffer buffer_;
		D3D12_INDEX_BUFFER_VIEW view_{};
		UINT count_=0, maximum_=0;
	};
}
