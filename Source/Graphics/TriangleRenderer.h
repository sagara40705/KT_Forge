#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Graphics/GraphicsDevice.h>
#include <Graphics/VertexBuffer.h>

namespace KT::Graphics
{
	class CommandContext;

	// 色付き三角形1個のRoot Signature、PSO、頂点バッファを所有する。
	// GPU完了まで保持する。提出・待機・状態遷移・Clearは担当しない。
	class TriangleRenderer : private KT::Core::NonCopyable
	{
	public:
		explicit TriangleRenderer(GraphicsDevice& device,
			DXGI_FORMAT targetFormat = DXGI_FORMAT_R8G8B8A8_UNORM);
		DXGI_FORMAT GetTargetFormat() const noexcept;

	private:
		friend class CommandContext;
		// CommandContextの記録状態・引数検査後にのみ呼ぶ。
		void Record(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
			UINT width, UINT height) const;

		ComPtr<ID3D12Device> device_{};
		ComPtr<ID3D12RootSignature> rootSignature_{};
		ComPtr<ID3D12PipelineState> pipelineState_{};
		VertexBuffer vertexBuffer_;
		DXGI_FORMAT targetFormat_;
	};
}
