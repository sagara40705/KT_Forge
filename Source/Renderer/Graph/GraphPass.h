#pragma once
#include <Renderer/Graph/GraphResourceUse.h>
#include <functional>
#include <string>
#include <vector>

namespace KT::Renderer
{
	class GraphExecutionContext;
	using GraphRecordFn = std::function<void(GraphExecutionContext&)>;

	// 画像の使用宣言と記録callback。Mesh/Material/PreparedRenderFrameを含めない。
	// callbackが借りるCPUデータはRecordまで不変。GPU所有者はFence完了まで保持。
	struct GraphPassDesc
	{
		std::string name;
		std::vector<GraphViewUse> views;
		std::vector<GraphResourceUse> resources;
	};
}
