#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Graphics/GraphicsDevice.h>
#include <Graphics/TriangleVertex.h>
#include <span>

namespace KT::Graphics
{
	// TriangleVertex専用の不変UPLOADバッファ。構築時に入力をコピーする。
	// GENERIC_READのまま使用する。記録開始からGPU完了まで所有者が保持する。
	class VertexBuffer : private KT::Core::NonCopyable
	{
	public:
		VertexBuffer(GraphicsDevice& device, std::span<const TriangleVertex> vertices);
		const D3D12_VERTEX_BUFFER_VIEW& GetView() const noexcept;

	private:
		ComPtr<ID3D12Resource> resource_{};
		D3D12_VERTEX_BUFFER_VIEW view_{};
	};
}
