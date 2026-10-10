#pragma once
#include <World/Scene/Scene.h>
#include <World/Scene/SceneAsset.h>
#include <World/Scene/ScriptRegistry.h>
#include <set>

namespace KT::World
{
	// 候補Scene内で参照できるUUIDと、固定済みScript登録を借用する。
	struct SceneReadContext
	{
		// 全オブジェクトのID集合。前方参照も復元前に検査できる。
		const std::set<ObjectUuid>& objects;
		// ScriptComponentの設定検査とC++実体の生成に使用する。
		const ScriptRegistry& scripts;
		// 全ゼロ値は参照未指定とし、有効なUUIDにはScene内の存在を要求する。
		void RequireReference(ObjectUuid uuid) const;
	};

	// 型を消した保存窓口。callbackは登録側の寿命より長い外部参照を捕捉しない。
	struct ComponentDescriptor
	{
		// ファイルから復元処理を選ぶ安定した文字列ID。
		std::string type;
		// World内のComponentから保存処理を選ぶ実行時の型識別。
		std::type_index cppType{typeid(void)};
		// この登録が読み書きできる設定version。
		std::uint32_t version = 1;
		// Componentを読み取り、保存する入力値だけをコピーする。
		std::function<SceneValue(const World&, Entity)> capture;
		// 検査・decodeは副作用を持たず、未知フィールドと不正値を拒否する。
		std::function<void(const SceneValue&, const SceneReadContext&)> validate;
		// 設定を復元し、候補SceneへのComponent追加を予約する。
		std::function<void(Scene&, EntityTarget, const SceneValue&, const SceneReadContext&)> restore;
	};

	// Component型と永続IDを対応づける。単一threadで所有者が登録し、読込前にFreezeする。
	class ComponentRegistry : private KT::Core::NonCopyable
	{
	public:
		// 設定と二重登録を検査した後に、登録表へ追加する。
		void Register(ComponentDescriptor descriptor);

		// decodeは検査と復元の両方で呼ぶ。副作用を持たせず、同じ設定から同じ値を作る。
		template <ComponentType T, class Encode, class Decode>
		void Register(std::string type, std::uint32_t version, Encode encode, Decode decode)
		{
			static_assert(!ReservedComponent<T> && !std::is_same_v<T, Name> && !std::is_same_v<T, ScriptComponent>);
			ComponentDescriptor descriptor;
			descriptor.type = std::move(type);
			descriptor.cppType = typeid(T);
			descriptor.version = version;
			descriptor.capture = [encode](const World& world, Entity entity) { return encode(world.GetComponent<T>(entity)); };
			descriptor.validate = [decode](const SceneValue& data, const SceneReadContext& context) { (void)decode(data, context); };
			descriptor.restore = [decode](Scene& scene, EntityTarget entity, const SceneValue& data, const SceneReadContext& context)
			{ scene.Commands().AddComponent<T>(entity, decode(data, context)); };
			Register(std::move(descriptor));
		}

		// 登録変更を止め、以降の読込・保存で同じ対応表を使用する。
		void Freeze() noexcept;
		[[nodiscard]] bool IsFrozen() const noexcept;
		[[nodiscard]] const ComponentDescriptor& Find(const std::string& type) const;
		[[nodiscard]] const ComponentDescriptor& Find(std::type_index cppType) const;
		void Validate(const SceneComponentDefinition& definition, const SceneReadContext& context) const;

	private:
		// 永続型ID順に登録情報を保持し、C++型の重複も登録時に検査する。
		std::map<std::string, ComponentDescriptor> entries_;
		// 固定後は登録情報への参照とcallbackを変更しない。
		bool frozen_ = false;
	};

	// 組込みComponentだけを明示登録する。ゲーム固有の型は起動側で追加する。
	void RegisterEngineComponents(ComponentRegistry& registry);
}
