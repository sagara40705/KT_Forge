#include <Renderer/Passes/UiPass.h>
#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Graph/GraphExecutionContext.h>

namespace KT::Renderer
{
	void AddUiPass(RenderGraph& graph, GraphViewHandle color, UiRenderer& ui)
	{
		GraphPassDesc pass;
		pass.name = "UiPass";
		pass.views = {{color, GraphResourceAccess::ReadWrite, GraphResourceUsage::RenderTarget}};
		graph.AddPass(std::move(pass), [color, &ui](GraphExecutionContext& context) { context.DrawUi(color, ui); });
	}
}
