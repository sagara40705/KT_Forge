#include <Graphics/Pipeline/GraphicsPipelineState.h>
#include <Graphics/GraphicsValidation.h>
#include <limits>
#include <stdexcept>

namespace KT::Graphics
{
	GraphicsPipelineState::GraphicsPipelineState(GraphicsDevice& device, const RootSignature& rootSignature, const Shader& vertexShader,
		const Shader& pixelShader, std::span<const D3D12_INPUT_ELEMENT_DESC> inputLayout, UINT vertexStride, DXGI_FORMAT targetFormat,
		DXGI_FORMAT depthFormat)
		: root_(rootSignature.GetSignature()),
		  stride_(vertexStride),
		  targetFormat_(targetFormat),
		  depthFormat_(depthFormat)
	{
		// shader stageと描画先のformatを確認する
		if (!IsColorTargetFormat(targetFormat) || depthFormat != DXGI_FORMAT_D32_FLOAT || vertexShader.GetStage() != ShaderStage::Vertex ||
			pixelShader.GetStage() != ShaderStage::Pixel)
		{
			throw std::invalid_argument("Pipeline shader/target/depth format is invalid.");
		}

		// 頂点レイアウトの要素数とstrideを確認する
		if (inputLayout.empty() || inputLayout.size() > D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT || !inputLayout.data() ||
			vertexStride == 0 || vertexStride > D3D12_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES)
		{
			throw std::invalid_argument("Pipeline input layout/stride is invalid.");
		}

		// 各頂点要素がstride内に収まるか確認する
		for (const auto& element : inputLayout)
		{
			const UINT elementSize = element.Format == DXGI_FORMAT_R32G32_FLOAT ? 8u :
				element.Format == DXGI_FORMAT_R32G32B32_FLOAT					? 12u :
				element.Format == DXGI_FORMAT_R32G32B32A32_FLOAT				? 16u :
																				  0u;
			if (!element.SemanticName || !*element.SemanticName || elementSize == 0 || element.InputSlot != 0 ||
				element.InputSlotClass != D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA || element.InstanceDataStepRate != 0 ||
				element.AlignedByteOffset % 4 != 0 || element.AlignedByteOffset > vertexStride ||
				elementSize > vertexStride - element.AlignedByteOffset)
			{
				throw std::invalid_argument("Pipeline input element is invalid.");
			}
		}

		// Root Signatureの所属Deviceを確認する
		RequireSameDevice(root_.Get(), device.GetDevice());
		rootCount_ = rootSignature.GetParameterCount();

		// Shader Model 6.0への対応を確認する
		D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{D3D_SHADER_MODEL_6_0};
		CheckGraphicsResult(device.GetDevice()->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)),
			"Shader model query failed.");
		if (shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_0)
		{
			throw std::runtime_error("Shader Model 6.0 required.");
		}

		// Root Signatureとshader・頂点入力を設定する
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = root_.Get();
		pipelineDesc.VS = vertexShader.GetBytecode();
		pipelineDesc.PS = pixelShader.GetBytecode();
		pipelineDesc.InputLayout = {inputLayout.data(), static_cast<UINT>(inputLayout.size())};

		// 塗りつぶしとclipを設定し、cullingを無効にする
		pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pipelineDesc.RasterizerState.DepthClipEnable = TRUE;

		// blend無効の描画と全color成分への書込みを設定する
		auto& blendTarget = pipelineDesc.BlendState.RenderTarget[0];
		blendTarget.SrcBlend = D3D12_BLEND_ONE;
		blendTarget.DestBlend = D3D12_BLEND_ZERO;
		blendTarget.BlendOp = D3D12_BLEND_OP_ADD;
		blendTarget.SrcBlendAlpha = D3D12_BLEND_ONE;
		blendTarget.DestBlendAlpha = D3D12_BLEND_ZERO;
		blendTarget.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendTarget.LogicOp = D3D12_LOGIC_OP_NOOP;
		blendTarget.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		// Reverse-ZのGREATER比較と深度書込みを設定する
		pipelineDesc.DepthStencilState.DepthEnable = TRUE;
		pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		pipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		pipelineDesc.DepthStencilState.StencilEnable = FALSE;

		// triangle-list・描画先format・sample1を設定する
		pipelineDesc.SampleMask = (std::numeric_limits<UINT>::max)();
		pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineDesc.NumRenderTargets = 1;
		pipelineDesc.RTVFormats[0] = targetFormat;
		pipelineDesc.DSVFormat = depthFormat;
		pipelineDesc.SampleDesc.Count = 1;

		// 設定した描画状態でPSOを作成する
		CheckGraphicsResult(device.GetDevice()->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(state_.GetAddressOf())),
			"Graphics PSO creation failed.");
	}
}
