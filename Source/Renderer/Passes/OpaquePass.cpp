#include <Renderer/Passes/OpaquePass.h>
#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Graph/GraphExecutionContext.h>
#include <Renderer/PreparedRenderFrame.h>
#include <Renderer/Mesh.h>
#include <Renderer/UnlitMaterial.h>
#include <Renderer/UnlitPipeline.h>
#include <Graphics/IndexedDrawPacket.h>
#include <Graphics/ConstantBufferArena.h>
#include <array>

namespace KT::Renderer
{
	// TODO: 未実装。PreparedFrameを値捕捉し、color/depthのReadWrite宣言とDraw callbackを登録する。
	void AddOpaquePass(RenderGraph& graph, GraphViewHandle color, GraphViewHandle depth,
		const PreparedRenderFrame& prepared)
	{
	}

}
