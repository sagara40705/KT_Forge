#pragma once
#include <World/Scene/Scene.h>
#include <World/Scene/SceneAsset.h>
#include <World/Scene/ComponentRegistry.h>

namespace KT::World
{
	// 固定Registryを通してCPU定義とWorldを変換する。JSON処理はSceneJsonへ分ける。
	class SceneLoader
	{
	public:
		// 固定した登録を借用し、設定の検査後に独立した候補Sceneを生成する。
		static void Validate(const SceneAsset& asset, const ComponentRegistry& components, const ScriptRegistry& scripts);
		[[nodiscard]] static std::unique_ptr<Scene> Load(
			const SceneAsset& asset, const ComponentRegistry& components, const ScriptRegistry& scripts);
		[[nodiscard]] static SceneAsset Capture(const Scene& scene, const ComponentRegistry& components, const ScriptRegistry& scripts);
		// 組込みComponentとMissing Scriptだけを扱う簡易窓口。登録表は呼出中だけ所有する。
		[[nodiscard]] static std::unique_ptr<Scene> Load(const SceneAsset& asset);
		[[nodiscard]] static SceneAsset Capture(const Scene& scene);
	};
}
