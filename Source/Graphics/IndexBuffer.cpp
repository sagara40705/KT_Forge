#include <Graphics/IndexBuffer.h>
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
			(void)ValidateIndexBufferLayout(bytes.size(),format);
			return bytes;
		}
	}
	IndexBuffer::IndexBuffer(GraphicsDevice& device, std::span<const std::byte> bytes, DXGI_FORMAT format)
		: buffer_(device,Validated(bytes,format))
	{
		const UINT stride=format==DXGI_FORMAT_R16_UINT?2u:4u;
		count_=static_cast<UINT>(bytes.size()/stride);
		for (std::size_t offset=0; offset<bytes.size(); offset+=stride)
		{
			UINT value=0;
			if (stride==2) { std::uint16_t index16; std::memcpy(&index16,bytes.data()+offset,2); value=index16; }
			else std::memcpy(&value,bytes.data()+offset,4);
			maximum_=(std::max)(maximum_,value);
		}
		view_={buffer_.GetResource()->GetGPUVirtualAddress(),static_cast<UINT>(bytes.size()),format};
	}
}
