#include <RuntimeIntegration/RenderExtractionSystem.h>
#include <stdexcept>

namespace KT::RuntimeIntegration
{
	RenderFrame RenderExtractionSystem::Extract(const KT::World::SceneUpdateContext& context) const
	{
		return Extract(context, context.GetFrame().camera);
	}

	RenderFrame RenderExtractionSystem::Extract(
		const KT::World::SceneUpdateContext& context, const std::optional<KT::World::CameraViewData>& camera) const
	{
		// 完成CPU結果を取得し、Camera未指定なら空の描画Frameを返す。
		const auto& state = context.GetCpuFrame();
		RenderFrame result;
		if (!camera)
		{
			return result;
		}

		// 同じCPU入力から求めたCamera値を、Rendererの所有値へコピーする。
		const auto& view = *camera;
		result.view = KT::Renderer::RenderView{
			view.view, view.projection, view.viewProjection, view.position, view.viewport.width, view.viewport.height};

		// 有効かつ表示対象のEntityだけを、アセットIDとWorld行列へ変換する。
		const auto& inputs = context.Inputs();
		for (std::size_t i = 0; i < state.entities.size(); ++i)
		{
			const auto& mesh = inputs[i].mesh;
			const auto& entity = state.entities[i];
			if (!entity.active.value || !mesh || !mesh->visible)
			{
				continue;
			}

			if (!mesh->meshId || !mesh->materialId)
			{
				throw std::invalid_argument("表示対象のMeshRendererのmeshIdまたはmaterialIdが0です。");
			}

			result.world.objects.push_back({mesh->meshId, mesh->materialId, entity.transform.matrix});
		}
		return result;
	}
}
