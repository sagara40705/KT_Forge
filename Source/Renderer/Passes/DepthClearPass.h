#pragma once
#include <Renderer/Graph/GraphViewHandle.h>

namespace KT::Renderer
{
	class RenderGraph;
	// 未実装: view全域のWriteAll/DepthStencil、D32 clear0を宣言・記録。
	void AddDepthClearPass(RenderGraph& graph, GraphViewHandle target);
}
