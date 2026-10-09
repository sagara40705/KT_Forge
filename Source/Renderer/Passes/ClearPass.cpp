#include <Renderer/Passes/ClearPass.h>
#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Graph/GraphExecutionContext.h>
#include <cmath>
#include <stdexcept>

namespace KT::Renderer
{
	// TODO: 未実装。finite色を検査し、WriteAll/RenderTarget宣言と値捕捉Clear callbackを登録する。
	void AddClearPass(RenderGraph& graph, GraphViewHandle target, std::array<float, 4> color)
	{
	}

}
