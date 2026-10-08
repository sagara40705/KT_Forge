#include <Graphics/RootSignature.h>
#include <Graphics/GraphicsValidation.h>
#include <stdexcept>

namespace KT::Graphics
{
	RootSignature::RootSignature(GraphicsDevice& device, std::span<const D3D12_ROOT_PARAMETER> parameters)
	{
		if (parameters.empty() || parameters.size()>32 || !parameters.data()) throw std::invalid_argument("Root parameter count is invalid.");
		// 初版はRootCBVだけ。table/range/samplerの借用寿命と別契約を混ぜない。
		for (const auto& p:parameters)
			if (p.ParameterType!=D3D12_ROOT_PARAMETER_TYPE_CBV || p.Descriptor.RegisterSpace!=0 ||
				(p.ShaderVisibility!=D3D12_SHADER_VISIBILITY_ALL && p.ShaderVisibility!=D3D12_SHADER_VISIBILITY_VERTEX && p.ShaderVisibility!=D3D12_SHADER_VISIBILITY_PIXEL))
				throw std::invalid_argument("Initial root signature supports root CBV in space0 only.");
		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.NumParameters=static_cast<UINT>(parameters.size()); desc.pParameters=parameters.data();
		desc.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		ComPtr<ID3DBlob> blob,errors;
		CheckGraphicsResult(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,blob.GetAddressOf(),errors.GetAddressOf()),"Root signature serialization failed.");
		CheckGraphicsResult(device.GetDevice()->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),
			IID_PPV_ARGS(signature_.GetAddressOf())),"Root signature creation failed.");
		count_=static_cast<UINT>(parameters.size());
	}
}
