#include <Graphics/Pipeline/RootSignature.h>
#include <Graphics/GraphicsValidation.h>
#include <stdexcept>

namespace KT::Graphics
{
	RootSignature::RootSignature(GraphicsDevice& device, std::span<const D3D12_ROOT_PARAMETER> parameters)
	{
		// root parameterの数と参照先を確認する
		if (parameters.empty() || parameters.size() > 32 || !parameters.data())
		{
			throw std::invalid_argument("Root parameterの数が1から32の範囲外です。");
		}
		// space0のCBVと、Pixel Shader用のSRV 1/2個またはSampler 1個に限定する。
		for (std::size_t index = 0; index < parameters.size(); ++index)
		{
			const auto& parameter = parameters[index];
			if (parameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV)
			{
				if (parameter.Descriptor.RegisterSpace != 0 ||
					(parameter.ShaderVisibility != D3D12_SHADER_VISIBILITY_ALL &&
						parameter.ShaderVisibility != D3D12_SHADER_VISIBILITY_VERTEX &&
						parameter.ShaderVisibility != D3D12_SHADER_VISIBILITY_PIXEL))
				{
					throw std::invalid_argument("Root CBVのspaceまたは可視stageが未対応です。");
				}
				kinds_[index] = RootBindingKind::Constant;
			}
			else if (parameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
			{
				if (parameter.ShaderVisibility != D3D12_SHADER_VISIBILITY_PIXEL || parameter.DescriptorTable.NumDescriptorRanges != 1 ||
					!parameter.DescriptorTable.pDescriptorRanges)
				{
					throw std::invalid_argument("Root tableはPixel Shaderの単一rangeだけに対応しています。");
				}
				const auto& range = parameter.DescriptorTable.pDescriptorRanges[0];
				if ((range.RangeType != D3D12_DESCRIPTOR_RANGE_TYPE_SRV && range.RangeType != D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER) ||
					(range.NumDescriptors != 1 && !(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV && range.NumDescriptors == 2)) ||
					range.RegisterSpace != 0 ||
					(range.OffsetInDescriptorsFromTableStart != 0 &&
						range.OffsetInDescriptorsFromTableStart != D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND))
				{
					throw std::invalid_argument("Root tableはspace0・先頭のSRV 1/2個またはSampler 1個が必要です。");
				}
				descriptorCounts_[index] = range.NumDescriptors;
				kinds_[index] = range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV ? RootBindingKind::Texture : RootBindingKind::Sampler;
			}
			else
			{
				throw std::invalid_argument("Root parameterの種類が未対応です。");
			}
		}

		// 入力レイアウトを受け取るRoot Signatureを設定する
		D3D12_ROOT_SIGNATURE_DESC signatureDesc{};
		signatureDesc.NumParameters = static_cast<UINT>(parameters.size());
		signatureDesc.pParameters = parameters.data();
		signatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		// 定義をシリアライズしてRoot Signatureを作成する
		ComPtr<ID3DBlob> serializedSignature, serializationErrors;
		CheckGraphicsResult(D3D12SerializeRootSignature(&signatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedSignature.GetAddressOf(),
								serializationErrors.GetAddressOf()),
			"Root Signatureのシリアライズに失敗しました。");
		CheckGraphicsResult(device.GetDevice()->CreateRootSignature(0, serializedSignature->GetBufferPointer(),
								serializedSignature->GetBufferSize(), IID_PPV_ARGS(signature_.GetAddressOf())),
			"Root Signatureの生成に失敗しました。");
		count_ = static_cast<UINT>(parameters.size());
	}
}
