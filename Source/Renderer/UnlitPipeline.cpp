#include <Renderer/UnlitPipeline.h>
#include <Renderer/Mesh.h>
#include <Unlit3DVS.h>
#include <Unlit3DSkinnedVS.h>
#include <Unlit3DPS.h>
#include <Unlit3DTexturedPS.h>
#include <Shadow3DPS.h>
#include <Shadow3DTexturedPS.h>
#include <array>
#include <cstddef>
#include <vector>

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

		// 既存Texture tableの番号を保ち、末尾にSkin CBVを追加する。
		std::vector<D3D12_ROOT_PARAMETER> Parameters(bool textured, bool skinned)
		{
			std::vector<D3D12_ROOT_PARAMETER> result(parameters.begin(), parameters.begin() + (textured ? 5 : 3));
			if (skinned)
			{
				D3D12_ROOT_PARAMETER skin{};
				skin.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				skin.Descriptor.ShaderRegister = 3;
				skin.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
				result.push_back(skin);
			}
			return result;
		}

		std::vector<D3D12_INPUT_ELEMENT_DESC> Layout(bool skinned)
		{
			std::vector<D3D12_INPUT_ELEMENT_DESC> result(layout.begin(), layout.end());
			if (skinned)
			{
				result.push_back({"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
				result.push_back({"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 64, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
			}
			return result;
		}
	}

	UnlitPipeline::UnlitPipeline(KT::Graphics::GraphicsDevice& device, DXGI_FORMAT target, DXGI_FORMAT depth, bool textured, bool skinned)
		: vertex_(KT::Graphics::ShaderStage::Vertex,
			  skinned ? std::span<const std::byte>(std::as_bytes(std::span{Unlit3DSkinnedVS})) :
						std::span<const std::byte>(std::as_bytes(std::span{Unlit3DVS}))),
		  pixel_(KT::Graphics::ShaderStage::Pixel,
			  target == DXGI_FORMAT_UNKNOWN ? (textured ? std::span<const std::byte>(std::as_bytes(std::span{Shadow3DTexturedPS})) :
														  std::span<const std::byte>(std::as_bytes(std::span{Shadow3DPS}))) :
				  textured					? std::span<const std::byte>(std::as_bytes(std::span{Unlit3DTexturedPS})) :
											  std::span<const std::byte>(std::as_bytes(std::span{Unlit3DPS}))),
		  root_(device, Parameters(textured, skinned)),
		  pipeline_(
			  device, root_, vertex_, pixel_, Layout(skinned), skinned ? sizeof(SkinnedMeshVertex) : sizeof(MeshVertex), target, depth)
	{
	}
}
