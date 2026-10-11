#include <Renderer/Mesh.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace KT::Renderer
{
	namespace
	{
		std::span<const std::byte> Validated(std::span<const MeshVertex> vertices)
		{
			if (vertices.empty() || !vertices.data())
			{
				throw std::invalid_argument("Meshの頂点が空です。");
			}
			if (vertices.size() > (std::numeric_limits<UINT>::max)() / sizeof(MeshVertex))
			{
				throw std::overflow_error("Meshの頂点byte数がUINT範囲を超えています。");
			}
			for (const auto& vertex : vertices)
			{
				for (float component : vertex.position)
				{
					if (!std::isfinite(component))
					{
						throw std::invalid_argument("Meshの頂点座標には有限値が必要です。");
					}
				}
				for (float component : vertex.texcoord)
				{
					if (!std::isfinite(component))
					{
						throw std::invalid_argument("MeshのUVには有限値が必要です。");
					}
				}
			}
			return std::as_bytes(vertices);
		}
	}

	Mesh::Mesh(
		KT::Graphics::GraphicsDevice& device, std::span<const MeshVertex> vertices, std::span<const std::byte> indices, DXGI_FORMAT format)
		: vertices_(device, Validated(vertices), sizeof(MeshVertex)),
		  indices_(device, indices, format)
	{
		if (indices_.GetMaximumIndex() >= vertices_.GetCount() || indices_.GetCount() % 3 != 0)
		{
			throw std::invalid_argument("MeshのIndex範囲または三角形のIndex数が不正です。");
		}
	}
}
