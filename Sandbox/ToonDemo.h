#pragma once
#include <Content/AssetCatalog.h>
#include <World/Scene/Scene.h>

namespace KT::Sandbox
{
	// 外部素材なしで静的/Skin・Toon/Unlit・床への影を確認する制作向けの例。
	void RegisterToonDemoAssets(KT::Content::AssetCatalog& catalog);
	std::unique_ptr<KT::World::Scene> CreateToonDemoScene();
}
