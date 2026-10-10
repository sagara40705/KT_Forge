#pragma once
#include <World/Scene/Scene.h>
#include <World/Scene/SceneAsset.h>

namespace KT::World
{
	// ファイル形式とEditor Registryは別の窓口。ここではCPU定義とWorldを変換する。
	class SceneLoader
	{
	public:
		// 設定から実体を生成する。未登録のクラスはnullptrを返して設定を残す。
		using ScriptFactory = std::function<std::unique_ptr<ScriptBehaviour>(const ScriptDefinition&)>;
		[[nodiscard]] static std::unique_ptr<Scene> Load(const SceneAsset& asset, const ScriptFactory& scripts = {});
		[[nodiscard]] static SceneAsset Capture(const Scene& scene);
	};
}
