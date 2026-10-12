#pragma once
#include <Content/AssetCatalog.h>
#include <World/Scene/Scene.h>
#include <memory>

namespace KT::Application
{
	// 同じ骨格素材を共有する四個体と静的Cubeの製品描画例。
	void RegisterAnimationDemoAssets(KT::Content::AssetCatalog& catalog);
	[[nodiscard]] std::unique_ptr<KT::World::Scene> CreateAnimationDemoScene();
}
