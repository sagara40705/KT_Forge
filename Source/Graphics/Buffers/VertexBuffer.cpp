#include <Graphics/Buffers/VertexBuffer.h>
#include <Graphics/GraphicsValidation.h>
#include <limits>
#include <stdexcept>

namespace KT::Graphics
{
	namespace
	{
		std::span<const std::byte> Validated(std::span<const std::byte> bytes, UINT stride)
		{
			// GPU資源の作成前に頂点サイズとstrideを確認する
			(void)ValidateVertexBufferLayout(bytes.size(), stride);
			return bytes;
		}
	}

	VertexBuffer::VertexBuffer(GraphicsDevice& device, std::span<const std::byte> bytes, UINT stride)
		: buffer_(device, Validated(bytes, stride))
	{
		// 頂点ビューと頂点数を保持する
		view_ = {buffer_.GetResource()->GetGPUVirtualAddress(), static_cast<UINT>(bytes.size()), stride};
		count_ = static_cast<UINT>(bytes.size() / stride);
	}
}
