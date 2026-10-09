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
			throw std::invalid_argument("Root parameter count is invalid.");
		}
		// 初版はRootCBVだけ。table/range/samplerの借用寿命と別契約を混ぜない。
		for (const auto& parameter : parameters)
		{
			if (parameter.ParameterType != D3D12_ROOT_PARAMETER_TYPE_CBV || parameter.Descriptor.RegisterSpace != 0 ||
				(parameter.ShaderVisibility != D3D12_SHADER_VISIBILITY_ALL &&
					parameter.ShaderVisibility != D3D12_SHADER_VISIBILITY_VERTEX &&
					parameter.ShaderVisibility != D3D12_SHADER_VISIBILITY_PIXEL))
			{
				throw std::invalid_argument("Initial root signature supports root CBV in space0 only.");
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
			"Root signature serialization failed.");
		CheckGraphicsResult(device.GetDevice()->CreateRootSignature(0, serializedSignature->GetBufferPointer(),
								serializedSignature->GetBufferSize(), IID_PPV_ARGS(signature_.GetAddressOf())),
			"Root signature creation failed.");
		count_ = static_cast<UINT>(parameters.size());
	}
}
