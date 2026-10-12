#include <Renderer/Passes/ShadowPass.h>
#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Graph/GraphExecutionContext.h>
#include <Renderer/PreparedRenderFrame.h>
#include <Renderer/Mesh.h>
#include <Renderer/UnlitMaterial.h>
#include <Graphics/Commands/IndexedDrawPacket.h>

namespace KT::Renderer
{
	void AddShadowPass(RenderGraph& graph, GraphViewHandle depth, const PreparedRenderFrame& prepared,
		KT::Graphics::ConstantSlice shadowView, unsigned resolution)
	{
		GraphPassDesc desc;
		desc.name = "DirectionalShadow";
		desc.views.push_back({depth, GraphResourceAccess::ReadWrite, GraphResourceUsage::DepthStencil});
		graph.AddPass(std::move(desc),
			[depth, snapshot = prepared, shadowView, resolution](GraphExecutionContext& context)
			{
				for (const auto& draw : snapshot.GetDraws())
				{
					const bool skinned = draw.mesh->SkinBindingCount() != 0;
					std::vector<KT::Graphics::RootConstantBinding> bindings{
						{0, shadowView}, {1, draw.objectConstants}, {2, draw.materialConstants}};
					const auto* texture = draw.material->GetTexture();
					if (skinned)
					{
						bindings.push_back({texture ? 5u : 3u, draw.skinConstants});
					}
					KT::Graphics::IndexedDrawPacket packet;
					packet.pipeline = &draw.material->GetShadowPipeline(skinned);
					packet.vertices = &draw.mesh->GetVertices();
					packet.indices = &draw.mesh->GetIndices();
					packet.bindings = bindings;
					packet.expectedWidth = packet.expectedHeight = resolution;
					packet.texture = texture;
					packet.sampler = texture ? draw.material->GetSampler() : nullptr;
					packet.textureRoot = 3;
					packet.samplerRoot = 4;
					packet.range = draw.range;
					context.DrawDepth(depth, packet);
				}
			});
	}
}
