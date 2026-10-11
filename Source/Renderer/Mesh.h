#pragma once
#include <Graphics/Buffers/VertexBuffer.h>
#include <Graphics/Buffers/IndexBuffer.h>
#include <array>
#include <type_traits>
#include <Content/MeshAsset.h>

namespace KT::Renderer
{
	// 不変VBの48byte配置。Unlitは位置とUV0を読み、法線・接線は将来の照明入力に保持する。
	struct MeshVertex
	{
		std::array<float, 3> position{};
		std::array<float, 2> texcoord{};
		std::array<float, 3> normal{};
		std::array<float, 4> tangent{};
	};

	static_assert(sizeof(MeshVertex) == 48 && std::is_standard_layout_v<MeshVertex> && std::is_trivially_copyable_v<MeshVertex>);

	// CPU属性をGPU頂点へ変換する。素材経路はDEFAULTで、Upload成功後だけ描画できる。
	class Mesh : private KT::Core::NonCopyable
	{
	public:
		Mesh(KT::Graphics::GraphicsDevice& device, std::span<const MeshVertex> vertices, std::span<const std::byte> indexBytes,
			DXGI_FORMAT indexFormat, KT::Graphics::BufferStorage storage = KT::Graphics::BufferStorage::Upload);
		// 素材はDEFAULTへ転送し、実際の最大IndexでR16/R32を選択する。
		Mesh(KT::Graphics::GraphicsDevice& device, const KT::Content::MeshAsset& source);
		void Upload(KT::Graphics::CommandQueue& queue);

		[[nodiscard]] bool IsReady() const noexcept
		{
			return vertices_.IsReady() && indices_.IsReady();
		}

		[[nodiscard]] const KT::Content::MeshSubmesh& Submesh(std::size_t index) const
		{
			return submeshes_.at(index);
		}

		[[nodiscard]] bool HasNormals() const noexcept
		{
			return hasNormals_;
		}

		[[nodiscard]] bool HasTangents() const noexcept
		{
			return hasTangents_;
		}

		[[nodiscard]] bool HasTexcoords() const noexcept
		{
			return hasTexcoords_;
		}

		const KT::Graphics::VertexBuffer& GetVertices() const noexcept
		{
			return vertices_;
		}

		const KT::Graphics::IndexBuffer& GetIndices() const noexcept
		{
			return indices_;
		}

	private:
		// constructor間で、選択済みIndex byte列をBuffer生成まで所有する。
		struct PackedIndices
		{
			std::vector<std::byte> bytes;
			DXGI_FORMAT format = DXGI_FORMAT_R32_UINT;
		};

		[[nodiscard]] static PackedIndices PackIndices(const KT::Content::MeshAsset& source);
		Mesh(KT::Graphics::GraphicsDevice& device, std::span<const MeshVertex> vertices, const PackedIndices& indices);

		KT::Graphics::VertexBuffer vertices_;
		KT::Graphics::IndexBuffer indices_;
		std::vector<KT::Content::MeshSubmesh> submeshes_;
		// 素材constructorで読み取った実属性の有無。互換raw入力には属性契約を推測しない。
		bool hasNormals_ = false, hasTangents_ = false, hasTexcoords_ = false;
	};
}
