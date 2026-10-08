#include <Renderer/Passes/ClearPass.h>
#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Graph/GraphExecutionContext.h>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace KT::Renderer
{
	// RenderGraphに全域Clearパスを追加する。targetはRenderTargetとして宣言されている必要がある
	void AddClearPass(RenderGraph& graph, GraphResourceHandle target, std::array<float, 4> color)
	{
		// 色成分の検査
		for (const auto& c : color)
		{
			if (std::isinf(c) || std::isnan(c))
			{
				throw std::invalid_argument("ClearPassの色成分にNaNまたはInfが含まれています。");
			}
		}

		// パスのDescを作成する
		GraphPassDesc desc{};
		desc.name = "ClearPass";

		GraphResourceUse use{};
		use.resource = target;
		use.access = GraphResourceAccess::WriteAll;
		use.usage = GraphResourceUsage::RenderTarget;
		desc.resources.push_back(use);

		// callbackと一緒に登録する
		graph.AddPass(std::move(desc),
			[target, color](GraphExecutionContext& context)
			{
				context.ClearColor(target, color);
			});


	}
}
