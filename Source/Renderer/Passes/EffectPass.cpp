#include <Renderer/Passes/EffectPass.h>
#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Graph/GraphExecutionContext.h>
#include <Renderer/Effects/EffectPlayer.h>
#include <Renderer/RenderData.h>

namespace KT::Renderer
{
	void AddEffectPass(RenderGraph& graph, GraphViewHandle color, GraphViewHandle depth, EffectPlayer& effects, const RenderView& view)
	{
		// Graphは画像だけを借用する。Playerと内部描画資源はFenceまで外部で保持する。
		GraphPassDesc pass;
		pass.name = "EffectPass";
		pass.views = {{color, GraphResourceAccess::ReadWrite, GraphResourceUsage::RenderTarget},
			{depth, GraphResourceAccess::Read, GraphResourceUsage::DepthStencilRead}};
		graph.AddPass(std::move(pass),
			[color, depth, &effects, view](GraphExecutionContext& context) { context.DrawEffects(color, depth, effects, view); });
	}
}
