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
	// 未実装契約: AddOpaquePassの本体を本人が実装。PreparedFrameはこの層だけが扱う。
	void AddOpaquePass(RenderGraph& graph, GraphViewHandle color, GraphViewHandle depth,
		const PreparedRenderFrame& prepared)
	{
	}

}
