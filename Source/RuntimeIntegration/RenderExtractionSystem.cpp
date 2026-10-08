#include <RuntimeIntegration/RenderExtractionSystem.h>
#include <stdexcept>

namespace KT::RuntimeIntegration
{
	RenderFrame RenderExtractionSystem::Extract(const KT::World::SceneUpdateContext& context) const
	{
		const auto& state = context.GetFrame();
		RenderFrame result;
		if (!state.camera) return result;
		const auto& view = *state.camera;
		result.view = KT::Renderer::RenderView{view.view, view.projection, view.viewProjection,
			view.position, view.viewport.width, view.viewport.height};
		const auto& inputs = context.Inputs();
		for (std::size_t i = 0; i < state.entities.size(); ++i)
		{
			const auto& mesh = inputs[i].mesh;
			const auto& entity = state.entities[i];
			if (!entity.active.value || !mesh || !mesh->visible) continue;
			if (!mesh->meshId || !mesh->materialId) throw std::invalid_argument("Visible mesh/material ID is zero.");
			result.world.objects.push_back({mesh->meshId, mesh->materialId, entity.transform.matrix});
		}
		return result;
	}
}
