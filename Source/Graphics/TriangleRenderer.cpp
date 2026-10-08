#include <Graphics/TriangleRenderer.h>
#include <TriangleVS.h>
#include <TrianglePS.h>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>

namespace KT::Graphics
{
	TriangleRenderer::TriangleRenderer(GraphicsDevice& device, DXGI_FORMAT targetFormat)
		: device_(device.GetDevice()), vertexBuffer_(device, TriangleVertices), targetFormat_(targetFormat)
	{
		if (targetFormat != DXGI_FORMAT_R8G8B8A8_UNORM && targetFormat != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB &&
			targetFormat != DXGI_FORMAT_B8G8R8A8_UNORM && targetFormat != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
		{
			throw std::invalid_argument("TriangleRendererはRGBA8/BGRA8のUNORMまたはSRGB RTVに対応します。");
		}
		D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{D3D_SHADER_MODEL_6_0};
		if (FAILED(device_->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel))) ||
			shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_0)
		{
			throw std::runtime_error("TriangleRendererにはShader Model 6.0対応が必要です。");
		}

		D3D12_ROOT_SIGNATURE_DESC root{};
		root.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		ComPtr<ID3DBlob> serialized{};
		ComPtr<ID3DBlob> errors{};
		if (FAILED(D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1,
			serialized.GetAddressOf(), errors.GetAddressOf())))
		{
			std::string message = "三角形Root Signatureのシリアライズに失敗しました。";
			if (errors)
				message.append(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
			throw std::runtime_error(message);
		}
		if (FAILED(device_->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
			IID_PPV_ARGS(rootSignature_.GetAddressOf()))))
		{
			throw std::runtime_error("三角形Root Signatureの作成に失敗しました。");
		}

		const D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
				static_cast<UINT>(offsetof(TriangleVertex, position)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
				static_cast<UINT>(offsetof(TriangleVertex, color)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline{};
		pipeline.pRootSignature = rootSignature_.Get();
		pipeline.VS = {TriangleVS, sizeof(TriangleVS)};
		pipeline.PS = {TrianglePS, sizeof(TrianglePS)};
		pipeline.InputLayout = {inputLayout, 2};
		pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pipeline.RasterizerState.DepthClipEnable = TRUE;
		auto& blend = pipeline.BlendState.RenderTarget[0];
		blend.SrcBlend = D3D12_BLEND_ONE;
		blend.DestBlend = D3D12_BLEND_ZERO;
		blend.BlendOp = D3D12_BLEND_OP_ADD;
		blend.SrcBlendAlpha = D3D12_BLEND_ONE;
		blend.DestBlendAlpha = D3D12_BLEND_ZERO;
		blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blend.LogicOp = D3D12_LOGIC_OP_NOOP;
		blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		pipeline.DepthStencilState.DepthEnable = FALSE;
		pipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		pipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		pipeline.DepthStencilState.StencilEnable = FALSE;
		pipeline.SampleMask = (std::numeric_limits<UINT>::max)();
		pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipeline.NumRenderTargets = 1;
		pipeline.RTVFormats[0] = targetFormat_;
		pipeline.SampleDesc.Count = 1;
		if (FAILED(device_->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(pipelineState_.GetAddressOf()))))
		{
			throw std::runtime_error("三角形Graphics PSOの作成に失敗しました。");
		}
	}

	DXGI_FORMAT TriangleRenderer::GetTargetFormat() const noexcept
	{
		return targetFormat_;
	}

	void TriangleRenderer::Record(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
		UINT width, UINT height) const
	{
		// 別デバイスの部品を記録しない。検査が終わるまでListに命令を追加しない。
		ComPtr<ID3D12Device> recordingDevice{};
		if (FAILED(list->GetDevice(IID_PPV_ARGS(recordingDevice.GetAddressOf()))))
			throw std::runtime_error("記録先のD3D12 Deviceを取得できません。");
		// COMの同一性は、双方のcanonical IUnknownで比較する。
		ComPtr<IUnknown> recordingIdentity{};
		ComPtr<IUnknown> rendererIdentity{};
		if (FAILED(recordingDevice->QueryInterface(IID_PPV_ARGS(recordingIdentity.GetAddressOf()))) ||
			FAILED(device_->QueryInterface(IID_PPV_ARGS(rendererIdentity.GetAddressOf()))))
			throw std::runtime_error("D3D12 DeviceのCOM同一性を確認できません。");
		if (recordingIdentity.Get() != rendererIdentity.Get())
			throw std::invalid_argument("TriangleRendererとCommandContextのDeviceが異なります。");

		const D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
		const D3D12_RECT scissor{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
		list->SetGraphicsRootSignature(rootSignature_.Get());
		list->SetPipelineState(pipelineState_.Get());
		list->RSSetViewports(1, &viewport);
		list->RSSetScissorRects(1, &scissor);
		list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		const auto& view = vertexBuffer_.GetView();
		list->IASetVertexBuffers(0, 1, &view);
		list->DrawInstanced(3, 1, 0, 0);
	}
}
