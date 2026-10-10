#pragma once
#include <World/Scene/SceneComponents.h>
#include <World/Scene/ScriptComponent.h>
#include <optional>

namespace KT::World
{
	// 保存・読込のCPU定義。Entityや実行中のScript実体は保存しない。
	struct SceneObjectDefinition
	{
		ObjectUuid uuid;
		std::string name;
		// 同じScene定義内の親UUID。未指定ならルートとして生成する。
		std::optional<ObjectUuid> parent;
		// 実行時にcomponentへ復元する入力値。未指定ならcomponentを追加しない。
		std::optional<ActiveSelf> active{ActiveSelf{}};
		std::optional<LocalTransform> transform{LocalTransform{}};
		std::optional<Camera> camera;
		std::optional<MeshRenderer> mesh;
		std::vector<ScriptDefinition> scripts;
	};

	// 新しいSceneとWorldを生成するための保存用定義。
	struct SceneAsset
	{
		std::string name;
		// 保存順に依存せず、UUIDで親参照を解決するオブジェクト定義。
		std::vector<SceneObjectDefinition> objects;
	};
}
