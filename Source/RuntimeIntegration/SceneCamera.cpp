#include <RuntimeIntegration/SceneCamera.h>
#include <stdexcept>

namespace KT::RuntimeIntegration
{
	std::optional<KT::World::Entity> SelectSceneCamera(const KT::World::Scene& scene, std::optional<KT::World::ObjectUuid> camera)
	{
		const auto& snapshot = scene.GetSnapshot();
		const auto& inputs = snapshot.Inputs();
		const auto& active = snapshot.GetActivation();
		std::optional<KT::World::Entity> selected;
		if (camera)
		{
			const auto entity = scene.FindByUuid(*camera);
			if (!entity)
			{
				throw std::invalid_argument("描画CameraのUUIDが現在のSceneにありません。");
			}
			const auto index = snapshot.GetHierarchy().FindNode(*entity);
			if (!inputs[index].camera || !active[index])
			{
				throw std::invalid_argument("指定した描画Cameraが無効か、Camera componentを持っていません。");
			}
			return entity;
		}
		for (std::size_t index = 0; index < inputs.size(); ++index)
		{
			if (inputs[index].camera && active[index])
			{
				if (selected)
				{
					throw std::invalid_argument("有効なCameraが複数あります。描画CameraのUUIDを明示指定してください。");
				}
				selected = inputs[index].entity;
			}
		}
		return selected;
	}
}
