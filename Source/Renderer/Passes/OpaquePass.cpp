#include <Renderer/Passes/OpaquePass.h>
#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Graph/GraphExecutionContext.h>
#include <Renderer/PreparedRenderFrame.h>
#include <Renderer/Mesh.h>
#include <Renderer/UnlitMaterial.h>
#include <Renderer/UnlitPipeline.h>
#include <Graphics/Commands/IndexedDrawPacket.h>
#include <Graphics/Buffers/ConstantBufferArena.h>
#include <array>
#include <stdexcept>

namespace KT::Renderer
{
	// PreparedFrameを値捕捉し、color/depthのReadWrite宣言とDraw callbackを登録する。
	void AddOpaquePass(RenderGraph& graph, GraphViewHandle color, GraphViewHandle depth, const PreparedRenderFrame& prepared)
	{
		// 前検査
		if (prepared.GetWidth() == 0 || prepared.GetHeight() == 0)
		{
			throw std::invalid_argument("PreparedRenderFrameの幅または高さが0です");
		}

		// パスの宣言を作る
		GraphPassDesc desc{};
		desc.name = "OpaquePass";

		GraphViewUse colorUse{};
		colorUse.view = color;
		colorUse.access = GraphResourceAccess::ReadWrite;
		colorUse.usage = GraphResourceUsage::RenderTarget;
		desc.views.push_back(colorUse);

		GraphViewUse depthUse{};
		depthUse.view = depth;
		depthUse.access = GraphResourceAccess::ReadWrite;
		depthUse.usage = GraphResourceUsage::DepthStencil;
		desc.views.push_back(depthUse);

		// コールバックと一緒に登録する
		graph.AddPass(std::move(desc),
			[color, depth, snapshot = prepared](GraphExecutionContext& context)
			{
				// 次の手順で、snapshot.GetDraws()を走査して描画する
				for (const auto& draw : snapshot.GetDraws())
				{
					if (!draw.mesh || !draw.material)
					{
						throw std::invalid_argument("PreparedDrawのmeshまたはmaterialがnullptrです");
					}
					// 描画のためのbindingを作成する
					const std::array<KT::Graphics::RootConstantBinding, 3> bindings{
						{{0, snapshot.GetViewConstants()}, {1, draw.objectConstants}, {2, draw.materialConstants}}};
					// IndexedDrawPacketを作成して、context.DrawIndexedに渡す
					KT::Graphics::IndexedDrawPacket packet{};
					packet.pipeline = &draw.material->GetPipeline().GetPipeline();
					packet.vertices = &draw.mesh->GetVertices();
					packet.indices = &draw.mesh->GetIndices();
					packet.bindings = bindings;
					packet.expectedWidth = snapshot.GetWidth();
					packet.expectedHeight = snapshot.GetHeight();

					// 描画を記録する
					context.DrawIndexed(color, depth, packet);
				}
			});
	}

}
