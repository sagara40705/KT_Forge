#include <Renderer/Lighting/ToonPipeline.h>
#include <Renderer/Mesh.h>
#include <Toon3DVS.h>
#include <Toon3DSkinnedVS.h>
#include <Toon3DPS.h>
#include <vector>

namespace KT::Renderer
{
	namespace
	{
		// t0/t1を連続SRV、s0を一つのSamplerとして固定する。
		const std::array<D3D12_DESCRIPTOR_RANGE, 2> ranges{
			{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, 0}, {D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0, 0}}};

		std::vector<D3D12_ROOT_PARAMETER> Parameters(bool skinned)
		{
			std::vector<D3D12_ROOT_PARAMETER> result(skinned ? 7 : 6);
			for (UINT index = 0; index < 3; ++index)
			{
				result[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				result[index].Descriptor.ShaderRegister = index;
				result[index].ShaderVisibility = index == 2 ? D3D12_SHADER_VISIBILITY_PIXEL : D3D12_SHADER_VISIBILITY_VERTEX;
			}
			for (UINT index = 0; index < 2; ++index)
			{
				result[index + 3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				result[index + 3].DescriptorTable = {1, &ranges[index]};
				result[index + 3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			}
			result[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			result[5].Descriptor.ShaderRegister = 4;
			result[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			if (skinned)
			{
				result[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				result[6].Descriptor.ShaderRegister = 3;
				result[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			}
			return result;
		}

		std::vector<D3D12_INPUT_ELEMENT_DESC> Layout(bool skinned)
		{
			std::vector<D3D12_INPUT_ELEMENT_DESC> result{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
			if (skinned)
			{
				result.push_back({"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
				result.push_back({"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 64, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
			}
			return result;
		}
	}

	ToonPipeline::ToonPipeline(KT::Graphics::GraphicsDevice& device, DXGI_FORMAT target, DXGI_FORMAT depth, bool skinned)
		: vertex_(KT::Graphics::ShaderStage::Vertex,
			  skinned ? std::span<const std::byte>(std::as_bytes(std::span{Toon3DSkinnedVS})) :
						std::span<const std::byte>(std::as_bytes(std::span{Toon3DVS}))),
		  pixel_(KT::Graphics::ShaderStage::Pixel, std::as_bytes(std::span{Toon3DPS})),
		  root_(device, Parameters(skinned)),
		  pipeline_(
			  device, root_, vertex_, pixel_, Layout(skinned), skinned ? sizeof(SkinnedMeshVertex) : sizeof(MeshVertex), target, depth)
	{
	}
}
