#pragma once
#include <World/Scene/Scene.h>

namespace KT::Application
{
	// ファイル指定なしの起動用に、組込みCubeとCameraを持つ小さなSceneを作る。
	[[nodiscard]] std::unique_ptr<KT::World::Scene> CreateStartupScene();
}
