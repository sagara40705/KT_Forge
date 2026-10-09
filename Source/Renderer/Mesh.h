#pragma once
#include <Graphics/Buffers/VertexBuffer.h>
#include <Graphics/Buffers/IndexBuffer.h>
#include <array>
#include <type_traits>

namespace KT::Renderer
{
	struct MeshVertex
	{
		std::array<float, 3> position{};
	};

	static_assert(sizeof(MeshVertex) == 12 && std::is_standard_layout_v<MeshVertex> && std::is_trivially_copyable_v<MeshVertex>);

	// CPU入力を検証後に不変VB/IBとして所有。UV/normal/texture/asset loaderは今回の範囲外。
	class Mesh : private KT::Core::NonCopyable
	{
	public:
		Mesh(KT::Graphics::GraphicsDevice& device, std::span<const MeshVertex> vertices, std::span<const std::byte> indexBytes,
			DXGI_FORMAT indexFormat);

		const KT::Graphics::VertexBuffer& GetVertices() const noexcept
		{
			return vertices_;
		}

		const KT::Graphics::IndexBuffer& GetIndices() const noexcept
		{
			return indices_;
		}

	private:
		KT::Graphics::VertexBuffer vertices_;
		KT::Graphics::IndexBuffer indices_;
	};
}
