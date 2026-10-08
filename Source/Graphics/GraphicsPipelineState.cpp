#include <Graphics/GraphicsPipelineState.h>
#include <Graphics/GraphicsValidation.h>
#include <limits>
#include <stdexcept>

namespace KT::Graphics
{
	GraphicsPipelineState::GraphicsPipelineState(GraphicsDevice& device, const RootSignature& root, const Shader& vs, const Shader& ps,
		std::span<const D3D12_INPUT_ELEMENT_DESC> layout, UINT stride, DXGI_FORMAT target, DXGI_FORMAT depth)
		: root_(root.GetSignature()),stride_(stride),targetFormat_(target),depthFormat_(depth)
	{
		if (!IsColorTargetFormat(target) || depth!=DXGI_FORMAT_D32_FLOAT || vs.GetStage()!=ShaderStage::Vertex || ps.GetStage()!=ShaderStage::Pixel)
			throw std::invalid_argument("Pipeline shader/target/depth format is invalid.");
		if (layout.empty() || layout.size()>D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT || !layout.data() ||
			stride==0 || stride>D3D12_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES)
			throw std::invalid_argument("Pipeline input layout/stride is invalid.");
		for (const auto& element:layout)
		{
			const UINT size=element.Format==DXGI_FORMAT_R32G32_FLOAT?8u:element.Format==DXGI_FORMAT_R32G32B32_FLOAT?12u:
				element.Format==DXGI_FORMAT_R32G32B32A32_FLOAT?16u:0u;
			if (!element.SemanticName || !*element.SemanticName || size==0 || element.InputSlot!=0 ||
				element.InputSlotClass!=D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA || element.InstanceDataStepRate!=0 ||
				element.AlignedByteOffset%4!=0 || element.AlignedByteOffset>stride || size>stride-element.AlignedByteOffset)
				throw std::invalid_argument("Pipeline input element is invalid.");
		}
		RequireSameDevice(root_.Get(),device.GetDevice());
		rootCount_=root.GetParameterCount();
		D3D12_FEATURE_DATA_SHADER_MODEL model{D3D_SHADER_MODEL_6_0};
		CheckGraphicsResult(device.GetDevice()->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,&model,sizeof(model)),"Shader model query failed.");
		if (model.HighestShaderModel<D3D_SHADER_MODEL_6_0) throw std::runtime_error("Shader Model 6.0 required.");
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature=root_.Get(); desc.VS=vs.GetBytecode(); desc.PS=ps.GetBytecode();
		desc.InputLayout={layout.data(),static_cast<UINT>(layout.size())};
		desc.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID; desc.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
		desc.RasterizerState.DepthClipEnable=TRUE;
		auto& blend=desc.BlendState.RenderTarget[0];
		blend.SrcBlend=D3D12_BLEND_ONE; blend.DestBlend=D3D12_BLEND_ZERO; blend.BlendOp=D3D12_BLEND_OP_ADD;
		blend.SrcBlendAlpha=D3D12_BLEND_ONE; blend.DestBlendAlpha=D3D12_BLEND_ZERO; blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;
		blend.LogicOp=D3D12_LOGIC_OP_NOOP; blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
		desc.DepthStencilState.DepthEnable=TRUE; desc.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_GREATER; desc.DepthStencilState.StencilEnable=FALSE;
		desc.SampleMask=(std::numeric_limits<UINT>::max)(); desc.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets=1; desc.RTVFormats[0]=target; desc.DSVFormat=depth; desc.SampleDesc.Count=1;
		CheckGraphicsResult(device.GetDevice()->CreateGraphicsPipelineState(&desc,IID_PPV_ARGS(state_.GetAddressOf())),"Graphics PSO creation failed.");
	}
}
