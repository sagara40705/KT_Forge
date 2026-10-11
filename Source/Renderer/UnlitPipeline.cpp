#include <Renderer/UnlitPipeline.h>
#include <Renderer/Mesh.h>
#include <Unlit3DVS.h>
#include <Unlit3DPS.h>
#include <Unlit3DTexturedPS.h>
#include <array>
#include <cstddef>

namespace KT::Renderer
{
	namespace
	{
		const std::array<D3D12_DESCRIPTOR_RANGE, 2> ranges{
			{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0}, {D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0, 0}}};
		const std::array<D3D12_ROOT_PARAMETER, 5> parameters = []
		{
			std::array<D3D12_ROOT_PARAMETER, 5> result{};
			for (UINT i = 0; i < 3; ++i)
			{
				result[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				result[i].Descriptor.ShaderRegister = i;
				result[i].ShaderVisibility = i == 2 ? D3D12_SHADER_VISIBILITY_PIXEL : D3D12_SHADER_VISIBILITY_VERTEX;
			}
			for (UINT index = 0; index < 2; ++index)
			{
				result[index + 3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				result[index + 3].DescriptorTable = {1, &ranges[index]};
				result[index + 3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			}
			return result;
		}();
		const std::array<D3D12_INPUT_ELEMENT_DESC, 2> layout{{{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
																  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(MeshVertex, texcoord), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}}};
	}

	UnlitPipeline::UnlitPipeline(KT::Graphics::GraphicsDevice& device, DXGI_FORMAT target, DXGI_FORMAT depth, bool textured)
		: vertex_(KT::Graphics::ShaderStage::Vertex, std::as_bytes(std::span{Unlit3DVS})),
		  pixel_(KT::Graphics::ShaderStage::Pixel,
			  textured ? std::span<const std::byte>(std::as_bytes(std::span{Unlit3DTexturedPS})) :
						 std::span<const std::byte>(std::as_bytes(std::span{Unlit3DPS}))),
		  root_(device, std::span(parameters).first(textured ? 5 : 3)),
		  pipeline_(device, root_, vertex_, pixel_, layout, sizeof(MeshVertex), target, depth)
	{
	}
}
