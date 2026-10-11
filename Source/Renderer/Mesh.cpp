#include <Renderer/Mesh.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace KT::Renderer
{
	namespace
	{
		std::vector<MeshVertex> Pack(const KT::Content::MeshAsset& source)
		{
			source.Validate();
			std::vector<MeshVertex> result;
			result.reserve(source.Positions().size());
			for (std::size_t index = 0; index < source.Positions().size(); ++index)
			{
				const auto position = source.Positions()[index];
				const auto uv = source.Texcoords().empty() ? KT::Core::Math::Vector2{} : source.Texcoords()[index];
				const auto normal = source.Normals().empty() ? KT::Core::Math::Vector3{} : source.Normals()[index];
				const auto tangent = source.Tangents().empty() ? std::array<float, 4>{} : source.Tangents()[index];
				result.push_back({{position.x, position.y, position.z}, {uv.x, uv.y}, {normal.x, normal.y, normal.z}, tangent});
			}
			return result;
		}

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
				for (float component : vertex.normal)
				{
					if (!std::isfinite(component))
					{
						throw std::invalid_argument("GPU Meshの法線には有限値が必要です。");
					}
				}
				for (float component : vertex.tangent)
				{
					if (!std::isfinite(component))
					{
						throw std::invalid_argument("GPU Meshの接線には有限値が必要です。");
					}
				}
			}
			return std::as_bytes(vertices);
		}
	}

	Mesh::Mesh(KT::Graphics::GraphicsDevice& device, std::span<const MeshVertex> vertices, std::span<const std::byte> indices,
		DXGI_FORMAT format, KT::Graphics::BufferStorage storage)
		: vertices_(device, Validated(vertices), sizeof(MeshVertex), storage),
		  indices_(device, indices, format, storage)
	{
		if (indices_.GetMaximumIndex() >= vertices_.GetCount() || indices_.GetCount() % 3 != 0)
		{
			throw std::invalid_argument("MeshのIndex範囲または三角形のIndex数が不正です。");
		}
		submeshes_.push_back({0, indices_.GetCount(), "default"});
	}

	Mesh::Mesh(KT::Graphics::GraphicsDevice& device, const KT::Content::MeshAsset& source)
		: Mesh(device, Pack(source), std::as_bytes(source.Indices()), DXGI_FORMAT_R32_UINT, KT::Graphics::BufferStorage::Default)
	{
		submeshes_.assign(source.Submeshes().begin(), source.Submeshes().end());
		hasNormals_ = !source.Normals().empty();
		hasTangents_ = !source.Tangents().empty();
		hasTexcoords_ = !source.Texcoords().empty();
	}

	void Mesh::Upload(KT::Graphics::CommandQueue& queue)
	{
		// 両Bufferの転送が完了してからMesh全体を描画可能とする。
		vertices_.Upload(queue);
		indices_.Upload(queue);
	}
}
