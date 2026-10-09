#include <BuiltinAssets/Meshes/CubeMesh.h>

namespace KT::BuiltinAssets
{
	std::unique_ptr<KT::Renderer::Mesh> CreateCubeMesh(KT::Graphics::GraphicsDevice& device)
	{
		constexpr std::array<KT::Renderer::MeshVertex, 8> vertices{{{{-.5f, -.5f, -.5f}}, {{.5f, -.5f, -.5f}}, {{.5f, .5f, -.5f}},
			{{-.5f, .5f, -.5f}}, {{-.5f, -.5f, .5f}}, {{.5f, -.5f, .5f}}, {{.5f, .5f, .5f}}, {{-.5f, .5f, .5f}}}};
		constexpr std::array<std::uint16_t, 36> indices{
			0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5, 3, 7, 6, 3, 6, 2, 0, 1, 5, 0, 5, 4};
		return std::make_unique<KT::Renderer::Mesh>(
			device, std::span<const KT::Renderer::MeshVertex>{vertices}, std::as_bytes(std::span{indices}), DXGI_FORMAT_R16_UINT);
	}
}
