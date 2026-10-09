#include <Renderer/Passes/DepthClearPass.h>
#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Graph/GraphExecutionContext.h>

namespace KT::Renderer
{
	// WriteAll/DepthStencil宣言とD32/Reverse-Z clear0のcallbackを登録する。
	void AddDepthClearPass(RenderGraph& graph, GraphViewHandle target)
	{
        // パスの宣言を作る
        GraphPassDesc desc{};
        desc.name = "DepthClearPass";
        GraphViewUse use{};
        use.view = target;
        use.access = GraphResourceAccess::WriteAll;
        use.usage = GraphResourceUsage::DepthStencil;
        desc.views.push_back(use);

        // ラムダと一緒に登録する
        graph.AddPass(std::move(desc),
            [target](GraphExecutionContext& context)
            {
                context.ClearDepth(target);
            });
	}
}
