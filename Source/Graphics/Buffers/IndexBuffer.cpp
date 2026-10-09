#include <Graphics/Buffers/IndexBuffer.h>
#include <Graphics/GraphicsValidation.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace KT::Graphics
{
	namespace
	{
		std::span<const std::byte> Validated(std::span<const std::byte> bytes, DXGI_FORMAT format)
		{
			// GPU資源の作成前にindex形式とデータサイズを確認する
			(void)ValidateIndexBufferLayout(bytes.size(), format);
			return bytes;
		}
	}

	IndexBuffer::IndexBuffer(GraphicsDevice& device, std::span<const std::byte> bytes, DXGI_FORMAT format)
		: buffer_(device, Validated(bytes, format))
	{
		// index形式から要素数を求める
		const UINT indexStride = format == DXGI_FORMAT_R16_UINT ? 2u : 4u;
		count_ = static_cast<UINT>(bytes.size() / indexStride);

		// 範囲外の頂点参照を検査するため最大indexを求める
		for (std::size_t offset = 0; offset < bytes.size(); offset += indexStride)
		{
			UINT indexValue = 0;
			if (indexStride == 2)
			{
				std::uint16_t index16;
				std::memcpy(&index16, bytes.data() + offset, 2);
				indexValue = index16;
			}
			else
			{
				std::memcpy(&indexValue, bytes.data() + offset, 4);
			}
			maximum_ = (std::max)(maximum_, indexValue);
		}

		// 全indexのビューを保持する
		view_ = {buffer_.GetResource()->GetGPUVirtualAddress(), static_cast<UINT>(bytes.size()), format};
	}
}
