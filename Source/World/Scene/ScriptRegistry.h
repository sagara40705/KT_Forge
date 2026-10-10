#pragma once
#include <Core/Utility/NonCopyable.h>
#include <World/Scene/SceneValue.h>
#include <World/Scene/ScriptComponent.h>
#include <functional>
#include <set>
#include <typeindex>

namespace KT::World
{
	// 起動側が所有するC++ Scriptの登録表。単一threadで読込開始前にFreezeする。
	class ScriptRegistry : private KT::Core::NonCopyable
	{
	public:
		// 設定検査は副作用を持たず、未定義項目と型不一致を拒否する。
		using ValidateSettings = std::function<void(const std::map<std::string, ScriptValue>&)>;
		// 検査済み設定からScript実体を生成し、所有権を呼出側へ渡す。
		using Factory = std::function<std::unique_ptr<ScriptBehaviour>(const ScriptDefinition&)>;

		// 自作C++型と永続クラスIDを明示登録し、双方の重複を拒否する。
		template <class T> void Register(std::string classId, std::uint32_t version, ValidateSettings validate, Factory factory)
		{
			static_assert(std::is_base_of_v<ScriptBehaviour, T>);
			RegisterEntry(std::move(classId), {typeid(T), version, std::move(validate), std::move(factory)});
		}

		// Scene読込前に登録を固定する。実行中にfactoryを差し替えない。
		void Freeze() noexcept;
		[[nodiscard]] bool IsFrozen() const noexcept;
		// 未登録クラスは設定を検査・保持し、実体生成だけを省略する。
		void Validate(const ScriptDefinition& definition, const std::set<ObjectUuid>& objects) const;
		// 未登録ならnullptrを返す。登録済みfactoryの生成失敗は例外にする。
		[[nodiscard]] std::unique_ptr<ScriptBehaviour> Create(
			const ScriptDefinition& definition, const std::set<ObjectUuid>& objects) const;
		// 設定値の型を明記し、Missing Scriptでも型と配列順を保持する。
		[[nodiscard]] static SceneValue Encode(const std::vector<ScriptDefinition>& definitions);
		// 保存定義だけを復元する。factoryとゲーム更新は呼ばない。
		[[nodiscard]] static std::vector<ScriptDefinition> Decode(const SceneValue& data);

	private:
		// C++型と永続クラスIDを結び、版ごとの設定検査と生成を行う。
		struct Entry
		{
			// 同じC++型を別のクラスIDで二重登録しないための識別。
			std::type_index cppType;
			// このクラスが受け付ける設定形式の版。
			std::uint32_t version;
			// クラス固有の必須項目・型・値域を検査する。
			ValidateSettings validate;
			// 実体生成だけを担い、Sceneへの登録はLoaderに任せる。
			Factory factory;
		};

		void RegisterEntry(std::string classId, Entry entry);
		// 永続クラスID順に、登録済みのC++型とcallbackを保持する。
		std::map<std::string, Entry> entries_;
		// 固定後はfactoryや検査callbackの参照が登録変更で失効しない。
		bool frozen_ = false;
	};
}
