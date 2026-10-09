#include <Renderer/Passes/ClearPass.h>
#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Graph/GraphExecutionContext.h>
#include <cmath>
#include <stdexcept>

namespace KT::Renderer
{
	// finite色を検査し、WriteAll/RenderTarget宣言と値捕捉Clear callbackを登録する。
	void AddClearPass(RenderGraph& graph, GraphViewHandle target, std::array<float, 4> color)
	{
        // 色を検査する
        for (const auto& c : color)
        {
            if (std::isnan(c) || std::isinf(c))
            {
                throw std::invalid_argument("Clear color contains NaN or Inf");
            }
        }

        // パスの宣言を作る
        GraphPassDesc desc{};
        desc.name = "ClearPass";
        GraphViewUse use{};
        use.view = target;
        use.access = GraphResourceAccess::WriteAll;
        use.usage = GraphResourceUsage::RenderTarget;
        desc.views.push_back(use);

        // コールバックと一緒に登録する
        graph.AddPass(std::move(desc),
            [target, color](GraphExecutionContext& context)
            {
                context.ClearColor(target, color);
            });
	}

}
