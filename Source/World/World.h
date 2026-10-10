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
	// const・volatileを含まない完全なobject型を受け付ける。
	// 配列と、例外を送出するデストラクタは使用しない。
	template <class T>
	concept ComponentType =
		std::is_object_v<T> && !std::is_array_v<T> && std::is_same_v<T, std::remove_cv_t<T>> && std::is_nothrow_destructible_v<T>;

	// Entityとcomponentを所有する。単一スレッドで使い、コピー・ムーブしない。
	// componentの構築・破棄中は、このWorldへの再入を拒否する。
	// 列挙・構造変更中にWorld自身を破棄すると、terminateする。
	// 返す参照やポインターは、そのcomponent・Entity・Worldの破棄まで有効。
	// 別のEntityやcomponentを追加・削除しても、既存componentは移動しない。
	class World : private KT::Core::NonCopyable
	{
	private:
		struct ComponentBase
		{
			virtual ~ComponentBase() = default;
		};

		template <ComponentType T> struct ComponentBox final : ComponentBase
		{
			template <class... Args>
			explicit ComponentBox(Args&&... args)
				: value(std::forward<Args>(args)...)
			{
			}

			T value;
		};

		struct EntityData
		{
			std::map<std::type_index, std::unique_ptr<ComponentBase>> components;
		};

		struct Slot
		{
			// 世代が上限に達したスロットは0として再利用しない。
			std::uint64_t generation = 1;
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
		// 生存Entityをスロット順にコピーする。component参照は保持しない。
		[[nodiscard]] std::vector<Entity> Entities() const;
		// 階層などの編集前に、列挙・構造変更中でないことを確認する。
		void RequireStructuralChange() const;
		[[nodiscard]] Entity CreateEntity();
		// 全componentを破棄する。失効・別World・二重破棄はinvalid_argument。
		void DestroyEntity(Entity entity);

		// 同じ型の追加はlogic_error。構築・確保に失敗した場合は登録しない。
		// コピー・ムーブできない型も、引数から直接構築する。
		template <ComponentType T, class... Args> T& AddComponent(Entity entity, Args&&... args)
		{
			// 構造変更を開始し、対象Entityの生存を確認する。
			MutationGuard guard(*this);
			RequireAlive(entity);

			auto& components = slots_[entity.index].data->components;
			const std::type_index type(typeid(T));
			if (components.contains(type))
			{
				throw std::logic_error("同じ型のcomponentが既に存在します。");
			}

			// 構築に成功したcomponentだけを登録する。
			auto componentBox = std::make_unique<ComponentBox<T>>(std::forward<Args>(args)...);
			auto* value = std::addressof(componentBox->value);
			components.emplace(type, std::move(componentBox));

			return *value;
		}

		// componentを借用する。無効・失効・別World・型なしはnullptr。
		template <ComponentType T> [[nodiscard]] T* FindComponent(Entity entity)
		{
			RequireReadable();
			return IsAliveUnchecked(entity) ? FindIn<T>(*slots_[entity.index].data) : nullptr;
		}

		template <ComponentType T> [[nodiscard]] const T* FindComponent(Entity entity) const
		{
			RequireReadable();
			return IsAliveUnchecked(entity) ? FindIn<T>(std::as_const(*slots_[entity.index].data)) : nullptr;
		}

		template <ComponentType T> [[nodiscard]] bool HasComponent(Entity entity) const
		{
			return FindComponent<T>(entity) != nullptr;
		}

		// Get・Removeは失効・別Worldにinvalid_argument、型なしにout_of_range。
		template <ComponentType T> [[nodiscard]] T& GetComponent(Entity entity)
		{
			RequireReadable();
			RequireAlive(entity);

			auto* value = FindIn<T>(*slots_[entity.index].data);
			if (!value)
			{
				throw std::out_of_range("要求したcomponentがありません。");
			}

			return *value;
		}

		template <ComponentType T> [[nodiscard]] const T& GetComponent(Entity entity) const
		{
			RequireReadable();
			RequireAlive(entity);

			const auto* value = FindIn<T>(std::as_const(*slots_[entity.index].data));
			if (!value)
			{
				throw std::out_of_range("要求したcomponentがありません。");
			}

			return *value;
		}

		template <ComponentType T> void RemoveComponent(Entity entity)
		{
			// 構造変更を開始し、対象Entityの生存を確認する。
			MutationGuard guard(*this);
			RequireAlive(entity);

			auto& components = slots_[entity.index].data->components;
			const auto componentIterator = components.find(std::type_index(typeid(T)));
			if (componentIterator == components.end())
			{
				throw std::out_of_range("要求したcomponentがありません。");
			}

			components.erase(componentIterator);
		}

		// 要求型をすべて持つEntityをスロット順に列挙する。型なしはコンパイル時に拒否する。
		// 値の編集・入れ子列挙は許可し、Create・Destroy・Add・Removeはlogic_error。
		// callbackの例外は伝播し、列挙状態を解除する。同じ型の重複指定は同じ参照を渡す。
		template <ComponentType... Ts, class Fn>
			requires(sizeof...(Ts) > 0 && std::invocable<Fn&, Entity, Ts&...>)
		void ForEach(Fn&& callback)
		{
			// 列挙中の構造変更を禁止し、生存スロットを順に調べる。
			EnumerationGuard guard(*this);

			for (std::size_t index = 0; index < slots_.size(); ++index)
			{
				if (!slots_[index].data)
				{
					continue;
				}

				auto& data = *slots_[index].data;
				if ((FindIn<Ts>(data) && ...))
				{
					std::invoke(callback, At(index), *FindIn<Ts>(data)...);
				}
			}
		}

		template <ComponentType... Ts, class Fn>
			requires(sizeof...(Ts) > 0 && std::invocable<Fn&, Entity, const Ts&...>)
		void ForEach(Fn&& callback) const
		{
			// 列挙中の構造変更を禁止し、生存スロットを順に調べる。
			EnumerationGuard guard(*this);

			for (std::size_t index = 0; index < slots_.size(); ++index)
			{
				if (!slots_[index].data)
				{
					continue;
				}

				const auto& data = std::as_const(*slots_[index].data);
				if ((FindIn<Ts>(data) && ...))
				{
					std::invoke(callback, At(index), *FindIn<Ts>(data)...);
				}
			}
		}

	private:
		template <ComponentType T> static T* FindIn(EntityData& data)
		{
			const auto componentIterator = data.components.find(std::type_index(typeid(T)));
			return componentIterator == data.components.end() ?
				nullptr :
				std::addressof(static_cast<ComponentBox<T>&>(*componentIterator->second).value);
		}

		template <ComponentType T> static const T* FindIn(const EntityData& data)
		{
			const auto componentIterator = data.components.find(std::type_index(typeid(T)));
			return componentIterator == data.components.end() ?
				nullptr :
				std::addressof(static_cast<const ComponentBox<T>&>(*componentIterator->second).value);
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
