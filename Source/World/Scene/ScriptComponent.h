#pragma once
#include <World/Entity.h>
#include <World/Scene/ObjectIdentity.h>
#include <map>
#include <memory>
#include <variant>
#include <vector>

namespace KT::World
{
	class Scene;
	class SceneUpdateContext;
	// 保存可能なScript設定値。オブジェクト参照はEntityではなくUUIDで表す。
	using ScriptValue = std::variant<bool, std::int64_t, double, std::string, ObjectUuid>;

	// Scene保存対象の設定。実行中のScript状態はここへ混ぜない。
	struct ScriptDefinition
	{
		// 読込時にScriptRegistryで検索する永続クラスID。
		std::string className;
		// 次のゲーム更新で対象を確定するときに参照する有効設定。
		bool enabled = true;
		// Scriptごとの設定値。実体を生成できなくても保持する。
		std::map<std::string, ScriptValue> settings;
		// 未登録クラスでも保存する設定形式のversion。
		std::uint32_t version = 1;
	};

	// Sceneの固定CPU入力を受け取り、値編集や構造変更の予約を行う実行窓口。
	class ScriptBehaviour
	{
	public:
		virtual ~ScriptBehaviour() = default;
		virtual void Update(Scene& scene, Entity entity, double deltaSeconds, const SceneUpdateContext& input) = 0;
	};

	// 保存設定と実行実体を対にして保持する。未登録Scriptは実体なしで残す。
	struct ScriptEntry
	{
		ScriptDefinition definition;
		// componentが所有するScript実体。生成できない場合はnullptr。
		std::unique_ptr<ScriptBehaviour> instance;
	};

	// Script実体はECSのcomponentが所有する。更新中は所有配列を編集できない。
	class ScriptComponent
	{
	public:
		explicit ScriptComponent(std::vector<ScriptEntry> entries);

		[[nodiscard]] const std::vector<ScriptEntry>& Entries() const noexcept
		{
			return entries_;
		}

	private:
		friend class Scene;
		// 登録順に更新する設定と実体。配列の編集はSceneが管理する。
		std::vector<ScriptEntry> entries_;
	};
}
