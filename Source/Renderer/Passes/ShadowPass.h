#pragma once
#include <Renderer/Graph/GraphViewHandle.h>
#include <Graphics/Buffers/ConstantBufferArena.h>

namespace KT::Renderer
{
	class RenderGraph;
	class PreparedRenderFrame;
	// Opaqueと同じObject/Skin定数で主光源の深度を描く。全域定義は先行Clearが担当する。
	void AddShadowPass(RenderGraph& graph, GraphViewHandle depth, const PreparedRenderFrame& prepared,
		KT::Graphics::ConstantSlice shadowView, unsigned resolution);
}
