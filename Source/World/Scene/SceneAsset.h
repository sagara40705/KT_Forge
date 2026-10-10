#pragma once
#include <World/Scene/ObjectIdentity.h>
#include <World/Scene/SceneValue.h>
#include <optional>

namespace KT::World
{
	// 安定した型IDと設定versionで、任意Componentの入力値を保持する。
	struct SceneComponentDefinition
	{
		// Registryで保存・復元処理を選ぶ永続ID。C++型名とは分ける。
		std::string type;
		// このComponentの設定構造の版。Scene全体のversionとは独立する。
		std::uint32_t version = 1;
		// 設定フィールドを所有する。Worldや外部メモリへの参照は持たない。
		SceneValue data{SceneValue::Object{}};
	};

	// 保存・読込のCPU定義。Entityや実行中のScript実体は保存しない。
	struct SceneObjectDefinition
	{
		// 再読込後も同じオブジェクトを識別する。実行時Entityは保存しない。
		ObjectUuid uuid;
		// 表示用の名前。参照の解決には使わず、重複を許可する。
		std::string name;
		// 同じScene定義内の親UUID。未指定ならルートとして生成する。
		std::optional<ObjectUuid> parent;
		// 存在するComponentだけを保存する。空配列ならComponentを追加しない。
		std::vector<SceneComponentDefinition> components;
	};

	// 新しいSceneとWorldを生成するための保存用定義。
	struct SceneAsset
	{
		// JSONと将来の配布用形式で共有するScene構造のversion。
		std::uint32_t version = 1;
		// Sceneの表示名。ファイルパスや実行時World IDとは独立する。
		std::string name;
		// 保存順に依存せず、UUIDで親参照を解決するオブジェクト定義。
		std::vector<SceneObjectDefinition> objects;
	};
}
