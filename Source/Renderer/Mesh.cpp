#include <Renderer/Mesh.h>
#include <algorithm>
#include <cstring>
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
		: Mesh(device, PackVertices(source), PackIndices(source))
	{
		submeshes_.assign(source.Submeshes().begin(), source.Submeshes().end());
		hasNormals_ = !source.Normals().empty();
		hasTangents_ = !source.Tangents().empty();
		hasTexcoords_ = !source.Texcoords().empty();
		skinBindingCount_ = source.Skin() ? source.Skin()->bindings.size() : 0;
	}

	Mesh::Mesh(KT::Graphics::GraphicsDevice& device, const PackedVertices& vertices, const PackedIndices& indices)
		: vertices_(device, vertices.bytes, vertices.stride, KT::Graphics::BufferStorage::Default),
		  indices_(device, indices.bytes, indices.format, KT::Graphics::BufferStorage::Default)
	{
		if (indices_.GetMaximumIndex() >= vertices_.GetCount() || indices_.GetCount() % 3 != 0)
		{
			throw std::invalid_argument("MeshのIndex範囲または三角形のIndex数が不正です。");
		}
		submeshes_.push_back({0, indices_.GetCount(), "default"});
	}

	Mesh::PackedVertices Mesh::PackVertices(const KT::Content::MeshAsset& source)
	{
		const auto vertices = Pack(source);
		PackedVertices result;
		if (!source.Skin())
		{
			const auto bytes = std::as_bytes(std::span(vertices));
			result.bytes.assign(bytes.begin(), bytes.end());
			return result;
		}

		// byte数を縮小する前に検査し、変換済み属性と同じ順で重みを追加する。
		if (vertices.size() > (std::numeric_limits<UINT>::max)() / sizeof(SkinnedMeshVertex))
		{
			throw std::overflow_error("Skin Meshの頂点byte数がUINT範囲を超えています。");
		}
		result.stride = sizeof(SkinnedMeshVertex);
		result.bytes.resize(vertices.size() * sizeof(SkinnedMeshVertex));
		for (std::size_t index = 0; index < vertices.size(); ++index)
		{
			const SkinnedMeshVertex vertex{vertices[index], source.Influences()[index]};
			std::memcpy(result.bytes.data() + index * sizeof(vertex), &vertex, sizeof(vertex));
		}
		return result;
	}

	Mesh::PackedIndices Mesh::PackIndices(const KT::Content::MeshAsset& source)
	{
		// 頂点数だけではなく参照される最大値を、縮小と資源作成より前に確認する。
		source.Validate();
		const auto indices = source.Indices();
		const auto maximum = *std::max_element(indices.begin(), indices.end());
		PackedIndices result;
		if (maximum <= (std::numeric_limits<std::uint16_t>::max)())
		{
			result.format = DXGI_FORMAT_R16_UINT;
			result.bytes.resize(indices.size() * sizeof(std::uint16_t));
			for (std::size_t index = 0; index < indices.size(); ++index)
			{
				const auto value = static_cast<std::uint16_t>(indices[index]);
				std::memcpy(result.bytes.data() + index * sizeof(value), &value, sizeof(value));
			}
		}
		else
		{
			const auto bytes = std::as_bytes(indices);
			result.bytes.assign(bytes.begin(), bytes.end());
		}
		return result;
	}

	void Mesh::Upload(KT::Graphics::CommandQueue& queue)
	{
		// 両Bufferの転送が完了してからMesh全体を描画可能とする。
		vertices_.Upload(queue);
		indices_.Upload(queue);
	}
}
