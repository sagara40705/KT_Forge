#pragma once
#include <Renderer/Graph/GraphViewHandle.h>

namespace KT::Renderer
{
	class RenderGraph;
	// view全域のWriteAll/DepthStencilとD32/Reverse-Z clear0を宣言・記録する。
	void AddDepthClearPass(RenderGraph& graph, GraphViewHandle target);
}
