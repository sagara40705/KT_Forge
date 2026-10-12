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
	void AddOpaquePass(
		RenderGraph& graph, GraphViewHandle color, GraphViewHandle depth, const PreparedRenderFrame& prepared, GraphViewHandle shadow)
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

		if (shadow.IsValid())
		{
			desc.views.push_back({shadow, GraphResourceAccess::Read, GraphResourceUsage::PixelShaderRead});
		}
		// コールバックと一緒に登録する
		graph.AddPass(std::move(desc),
			[color, depth, shadow, snapshot = prepared](GraphExecutionContext& context)
			{
				// 次の手順で、snapshot.GetDraws()を走査して描画する
				for (const auto& draw : snapshot.GetDraws())
				{
					if (!draw.mesh || !draw.material)
					{
						throw std::invalid_argument("PreparedDrawのmeshまたはmaterialがnullptrです");
					}
					// 描画のためのbindingを作成する
					std::vector<KT::Graphics::RootConstantBinding> bindings{
						{0, snapshot.GetViewConstants()}, {1, draw.objectConstants}, {2, draw.materialConstants}};
					const bool skinned = draw.mesh->SkinBindingCount() != 0;
					const bool toon = draw.material->IsToon();
					if (toon)
					{
						bindings.push_back({5, snapshot.GetLightConstants()});
					}
					if (skinned)
					{
						bindings.push_back({toon ? 6u : draw.material->GetTexture() ? 5u : 3u, draw.skinConstants});
					}
					// IndexedDrawPacketを作成して、context.DrawIndexedに渡す
					KT::Graphics::IndexedDrawPacket packet{};
					packet.pipeline = &draw.material->GetSurfacePipeline(skinned);
					packet.vertices = &draw.mesh->GetVertices();
					packet.indices = &draw.mesh->GetIndices();
					packet.bindings = bindings;
					packet.texture = toon ? nullptr : draw.material->GetTexture();
					packet.texturePair = draw.texturePair;
					packet.sampler = draw.material->GetSampler();
					packet.textureRoot = 3;
					packet.samplerRoot = 4;
					packet.expectedWidth = snapshot.GetWidth();
					packet.expectedHeight = snapshot.GetHeight();
					packet.range = draw.range;

					// 描画を記録する
					if (toon)
					{
						context.DrawToon(color, depth, shadow, packet);
					}
					else
					{
						context.DrawIndexed(color, depth, packet);
					}
				}
			});
	}

}
