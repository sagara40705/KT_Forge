#include <Renderer/Passes/ClearPass.h>
#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Graph/GraphExecutionContext.h>
#include <cmath>
#include <stdexcept>

namespace KT::Renderer
{
	// 未実装契約: AddClearPassの本体を本人が実装。旧ResourceHandle版は原本に退避。
	void AddClearPass(RenderGraph& graph, GraphViewHandle target, std::array<float, 4> color)
	{
	}

}
