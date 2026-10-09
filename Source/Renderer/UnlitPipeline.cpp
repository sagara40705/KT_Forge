#include <Renderer/UnlitPipeline.h>
#include <Renderer/Mesh.h>
#include <Unlit3DVS.h>
#include <Unlit3DPS.h>
#include <array>
#include <cstddef>

namespace KT::Renderer
{
	namespace
	{
		const std::array<D3D12_ROOT_PARAMETER, 3> parameters = []
		{
			std::array<D3D12_ROOT_PARAMETER, 3> result{};
			for (UINT i = 0; i < 3; ++i)
			{
				result[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				result[i].Descriptor.ShaderRegister = i;
				result[i].ShaderVisibility = i == 2 ? D3D12_SHADER_VISIBILITY_PIXEL : D3D12_SHADER_VISIBILITY_VERTEX;
			}
			return result;
		}();
		const std::array<D3D12_INPUT_ELEMENT_DESC, 1> layout{
			{{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}}};
	}

	UnlitPipeline::UnlitPipeline(KT::Graphics::GraphicsDevice& device, DXGI_FORMAT target, DXGI_FORMAT depth)
		: vertex_(KT::Graphics::ShaderStage::Vertex, std::as_bytes(std::span{Unlit3DVS})),
		  pixel_(KT::Graphics::ShaderStage::Pixel, std::as_bytes(std::span{Unlit3DPS})),
		  root_(device, parameters),
		  pipeline_(device, root_, vertex_, pixel_, layout, sizeof(MeshVertex), target, depth)
	{
	}
}
