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
			if (vertices.empty() || !vertices.data()) throw std::invalid_argument("Mesh vertices are empty.");
			if (vertices.size()>(std::numeric_limits<UINT>::max)()/sizeof(MeshVertex)) throw std::overflow_error("Mesh vertices overflow.");
			for (const auto& v:vertices) for (float component:v.position)
				if (!std::isfinite(component)) throw std::invalid_argument("Mesh position is nonfinite.");
			return std::as_bytes(vertices);
		}
	}
	Mesh::Mesh(KT::Graphics::GraphicsDevice& device, std::span<const MeshVertex> vertices,
		std::span<const std::byte> indices, DXGI_FORMAT format)
		: vertices_(device,Validated(vertices),sizeof(MeshVertex)),indices_(device,indices,format)
	{
		if (indices_.GetMaximumIndex()>=vertices_.GetCount() || indices_.GetCount()%3!=0)
			throw std::invalid_argument("Mesh index range/topology is invalid.");
	}
}
