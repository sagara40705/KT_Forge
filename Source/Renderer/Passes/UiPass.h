#pragma once
#include <Renderer/Graph/GraphViewHandle.h>

namespace KT::Renderer
{
	class RenderGraph;
	class UiRenderer;
	// Scene/Effect完成後の色だけへ合成する。RendererをGPU完了まで外部で保持する。
	void AddUiPass(RenderGraph& graph, GraphViewHandle color, UiRenderer& ui);
}
