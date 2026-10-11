#pragma once
#include <World/Scene/Scene.h>

namespace KT::Application
{
	// 製品のScene・素材・Rendererを通る落下例。物理専用の描画経路は持たない。
	std::unique_ptr<KT::World::Scene> CreatePhysicsDemoScene();
}
