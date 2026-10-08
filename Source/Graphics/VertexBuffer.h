#pragma once
#include <Graphics/GpuBuffer.h>

namespace KT::Graphics
{
	// 頂点型を知らない不変VB。layout/位置値の意味検証はMesh側。
	class VertexBuffer : private KT::Core::NonCopyable
	{
	public:
		VertexBuffer(GraphicsDevice& device, std::span<const std::byte> bytes, UINT stride);
		const D3D12_VERTEX_BUFFER_VIEW& GetView() const noexcept { return view_; }
		UINT GetCount() const noexcept { return count_; }
		ID3D12Resource* GetResource() const noexcept { return buffer_.GetResource(); }
	private:
		GpuBuffer buffer_;
		D3D12_VERTEX_BUFFER_VIEW view_{};
		UINT count_=0;
	};
}