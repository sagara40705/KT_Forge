#pragma once
#include <Graphics/Pipeline/RootSignature.h>
#include <Graphics/Pipeline/Shader.h>

namespace KT::Graphics
{
	// 初版は不透明triangle-list/sample1/Reverse-Z GREATER。layoutとformatを検証しPSOを所有。
	class GraphicsPipelineState : private KT::Core::NonCopyable
	{
	public:
		GraphicsPipelineState(GraphicsDevice& device, const RootSignature& rootSignature, const Shader& vertexShader,
			const Shader& pixelShader, std::span<const D3D12_INPUT_ELEMENT_DESC> inputLayout, UINT vertexStride, DXGI_FORMAT targetFormat,
			DXGI_FORMAT depthFormat);

		ID3D12PipelineState* GetState() const noexcept
		{
			return state_.Get();
		}

		ID3D12RootSignature* GetRootSignature() const noexcept
		{
			return root_.Get();
		}

		UINT GetVertexStride() const noexcept
		{
			return stride_;
		}

		DXGI_FORMAT GetTargetFormat() const noexcept
		{
			return targetFormat_;
		}

		DXGI_FORMAT GetDepthFormat() const noexcept
		{
			return depthFormat_;
		}

		UINT GetRootParameterCount() const noexcept
		{
			return rootCount_;
		}

		[[nodiscard]] RootBindingKind GetRootBindingKind(UINT index) const
		{
			return kinds_.at(index);
		}

		[[nodiscard]] UINT GetDescriptorCount(UINT index) const
		{
			return descriptorCounts_.at(index);
		}

	private:
		ComPtr<ID3D12PipelineState> state_;
		ComPtr<ID3D12RootSignature> root_; // PSOから借用するRootSignatureの寿命も明示所有で保証。
		UINT stride_;
		UINT rootCount_ = 0;
		std::array<RootBindingKind, 32> kinds_{};
		// Root tableの範囲長を保持し、Draw前に画像数を検査する。
		std::array<UINT, 32> descriptorCounts_{};
		DXGI_FORMAT targetFormat_, depthFormat_;
	};
}
