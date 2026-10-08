#include <Graphics/VertexBuffer.h>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace KT::Graphics
{
	VertexBuffer::VertexBuffer(GraphicsDevice& device, std::span<const TriangleVertex> vertices)
	{
		if (vertices.empty())
		{
			throw std::invalid_argument("VertexBufferの頂点が空です。");
		}
		// view.SizeInBytesへ縮小変換する前に、乗算も含めて範囲を確認する。
		if (vertices.size() > (std::numeric_limits<UINT>::max)() / sizeof(TriangleVertex))
		{
			throw std::overflow_error("VertexBufferのサイズがUINTの範囲を超えます。");
		}
		for (const auto& vertex : vertices)
		{
			for (const auto value : vertex.position)
			{
				if (!std::isfinite(value))
					throw std::invalid_argument("頂点位置にNaNまたはInfがあります。");
			}
			for (const auto value : vertex.color)
			{
				if (!std::isfinite(value))
					throw std::invalid_argument("頂点色にNaNまたはInfがあります。");
			}
		}

		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_UPLOAD;
		heap.CreationNodeMask = 1;
		heap.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = vertices.size_bytes();
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		if (FAILED(device.GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
			&desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(resource_.GetAddressOf()))))
		{
			throw std::runtime_error("VertexBufferの作成に失敗しました。");
		}

		void* mapped = nullptr;
		const D3D12_RANGE readRange{0, 0}; // CPUは読まない。
		if (FAILED(resource_->Map(0, &readRange, &mapped)))
		{
			throw std::runtime_error("VertexBufferのMapに失敗しました。");
		}
		std::memcpy(mapped, vertices.data(), vertices.size_bytes());
		const D3D12_RANGE writtenRange{0, vertices.size_bytes()};
		resource_->Unmap(0, &writtenRange);

		view_.BufferLocation = resource_->GetGPUVirtualAddress();
		view_.SizeInBytes = static_cast<UINT>(vertices.size_bytes());
		view_.StrideInBytes = static_cast<UINT>(sizeof(TriangleVertex));
	}

	const D3D12_VERTEX_BUFFER_VIEW& VertexBuffer::GetView() const noexcept
	{
		return view_;
	}
}
