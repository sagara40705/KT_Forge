#pragma once
#include <World/Scene/Scene.h>

namespace KT::RuntimeIntegration
{
	// UUID指定時はそのCameraだけを使う。未指定なら完成Snapshot内の有効な1台を選ぶ。
	// 0台ならClearだけ、複数なら明示指定を要求する。失効・無効Cameraへ暗黙fallbackしない。
	[[nodiscard]] std::optional<KT::World::Entity> SelectSceneCamera(
		const KT::World::Scene& scene, std::optional<KT::World::ObjectUuid> camera = {});
}
