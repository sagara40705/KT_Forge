#pragma once
#include <Core/Utility/NonCopyable.h>
#include <World/Entity.h>
#include <concepts>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace KT::World
{
	// 完全な非const/非volatile object型。配列とthrowするデストラクタは非対応。
	template<class T>
	concept ComponentType = std::is_object_v<T> && !std::is_array_v<T> &&
		std::is_same_v<T, std::remove_cv_t<T>> && std::is_nothrow_destructible_v<T>;

	// Entityとcomponentの唯一の所有者。単一threadで使用し、copy/moveしない。
	// componentの構築/破棄中はこのWorldへ再入しない。
	// 列挙/構造変更の実行中にWorld自身を破棄するとterminateする。
	// 返す参照/pointerは同じcomponentのRemove、EntityのDestroy、Worldの破棄まで有効。
	// 他のEntity/componentの追加削除では、component自体を移動しない。
	class World : private KT::Core::NonCopyable
	{
	private:
		struct ComponentBase
		{
			virtual ~ComponentBase() = default;
		};
		template<ComponentType T>
		struct ComponentBox final : ComponentBase
		{
			template<class... Args>
			explicit ComponentBox(Args&&... args) : value(std::forward<Args>(args)...) {}
			T value;
		};
		struct EntityData
		{
			std::map<std::type_index, std::unique_ptr<ComponentBase>> components;
		};
		struct Slot
		{
			std::uint64_t generation = 1; // 0は上限到達で退役したslot。
			std::unique_ptr<EntityData> data{};
		};
		class MutationGuard : private KT::Core::NonCopyable
		{
		public:
			explicit MutationGuard(World& world);
			~MutationGuard();
		private:
			World& world_;
		};
		class EnumerationGuard : private KT::Core::NonCopyable
		{
		public:
			explicit EnumerationGuard(const World& world);
			~EnumerationGuard();
		private:
			const World& world_;
		};

	public:
		World();
		~World();
		[[nodiscard]] std::uint64_t Identity() const;
		[[nodiscard]] std::size_t Count() const;
		[[nodiscard]] bool IsAlive(Entity entity) const;
		// 生存Entityのslot順snapshot。component参照を保持しない。
		[[nodiscard]] std::vector<Entity> Entities() const;
		// Hierarchy等の編集API用。列挙/構造変更中ならlogic_error。
		void RequireStructuralChange() const;
		[[nodiscard]] Entity CreateEntity();
		// 失効/別World/二重Destroyはinvalid_argument。componentを全て破棄する。
		void DestroyEntity(Entity entity);

		// 同じ型の重複Addはlogic_error。構築/確保失敗時は登録しない。
		// move-onlyやimmovableのTも、constructor引数で直接構築できる。
		template<ComponentType T, class... Args>
		T& AddComponent(Entity entity, Args&&... args)
		{
			MutationGuard guard(*this);
			RequireAlive(entity);
			auto& components = slots_[entity.index].data->components;
			const std::type_index type(typeid(T));
			if (components.contains(type))
				throw std::logic_error("同じ型のcomponentが既に存在します。");
			auto box = std::make_unique<ComponentBox<T>>(std::forward<Args>(args)...);
			auto* value = std::addressof(box->value);
			components.emplace(type, std::move(box));
			return *value;
		}

		// 無効/失効/別World/型なしはいずれもnullptr。内部を借用する。
		template<ComponentType T>
		[[nodiscard]] T* FindComponent(Entity entity)
		{
			RequireReadable();
			return IsAliveUnchecked(entity) ? FindIn<T>(*slots_[entity.index].data) : nullptr;
		}
		template<ComponentType T>
		[[nodiscard]] const T* FindComponent(Entity entity) const
		{
			RequireReadable();
			return IsAliveUnchecked(entity) ? FindIn<T>(std::as_const(*slots_[entity.index].data)) : nullptr;
		}
		template<ComponentType T>
		[[nodiscard]] bool HasComponent(Entity entity) const
		{
			return FindComponent<T>(entity) != nullptr;
		}

		// Get/Removeは失効/別Worldにinvalid_argument、型なしにout_of_range。
		template<ComponentType T>
		[[nodiscard]] T& GetComponent(Entity entity)
		{
			RequireReadable();
			RequireAlive(entity);
			auto* value = FindIn<T>(*slots_[entity.index].data);
			if (!value) throw std::out_of_range("要求したcomponentがありません。");
			return *value;
		}
		template<ComponentType T>
		[[nodiscard]] const T& GetComponent(Entity entity) const
		{
			RequireReadable();
			RequireAlive(entity);
			const auto* value = FindIn<T>(std::as_const(*slots_[entity.index].data));
			if (!value) throw std::out_of_range("要求したcomponentがありません。");
			return *value;
		}
		template<ComponentType T>
		void RemoveComponent(Entity entity)
		{
			MutationGuard guard(*this);
			RequireAlive(entity);
			auto& components = slots_[entity.index].data->components;
			const auto found = components.find(std::type_index(typeid(T)));
			if (found == components.end()) throw std::out_of_range("要求したcomponentがありません。");
			components.erase(found);
		}

		// 要求型全てを持つEntityをslot順に列挙。空queryはコンパイル時に拒否。
		// 値編集/入れ子列挙は可能、Create/Destroy/Add/Removeはlogic_error。
		// callback例外は伝播するがguardは解除する。同じ型の重複指定は同じ参照を渡す。
		template<ComponentType... Ts, class Fn>
			requires (sizeof...(Ts) > 0 && std::invocable<Fn&, Entity, Ts&...>)
		void ForEach(Fn&& callback)
		{
			EnumerationGuard guard(*this);
			for (std::size_t index = 0; index < slots_.size(); ++index)
			{
				if (!slots_[index].data) continue;
				auto& data = *slots_[index].data;
				if ((FindIn<Ts>(data) && ...))
					std::invoke(callback, At(index), *FindIn<Ts>(data)...);
			}
		}
		template<ComponentType... Ts, class Fn>
			requires (sizeof...(Ts) > 0 && std::invocable<Fn&, Entity, const Ts&...>)
		void ForEach(Fn&& callback) const
		{
			EnumerationGuard guard(*this);
			for (std::size_t index = 0; index < slots_.size(); ++index)
			{
				if (!slots_[index].data) continue;
				const auto& data = std::as_const(*slots_[index].data);
				if ((FindIn<Ts>(data) && ...))
					std::invoke(callback, At(index), *FindIn<Ts>(data)...);
			}
		}

	private:
		template<ComponentType T>
		static T* FindIn(EntityData& data)
		{
			const auto found = data.components.find(std::type_index(typeid(T)));
			return found == data.components.end() ? nullptr : std::addressof(static_cast<ComponentBox<T>&>(*found->second).value);
		}
		template<ComponentType T>
		static const T* FindIn(const EntityData& data)
		{
			const auto found = data.components.find(std::type_index(typeid(T)));
			return found == data.components.end() ? nullptr : std::addressof(static_cast<const ComponentBox<T>&>(*found->second).value);
		}
		void RequireReadable() const;
		void RequireAlive(Entity entity) const;
		bool IsAliveUnchecked(Entity entity) const noexcept;
		Entity At(std::size_t index) const noexcept;

		const std::uint64_t worldId_;
		std::vector<Slot> slots_{};
		std::size_t count_ = 0;
		mutable std::size_t enumerations_ = 0;
		bool mutating_ = false;
	};
}
