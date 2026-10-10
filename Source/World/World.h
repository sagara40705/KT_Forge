#pragma once
#include <Core/Utility/NonCopyable.h>
#include <World/Entity.h>
#include <World/Scene/ObjectIdentity.h>
#include <World/Scene/RollbackState.h>
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace KT::World
{
	struct Hierarchy;
	class ScriptComponent;
	class ScriptBehaviour;
	class WorldCommandBuffer;
	class Scene;

	// 親変更で維持する変換を指定する。KeepWorldは反映時点の行列をTRSで保持する。
	enum class ParentChangeMode
	{
		KeepLocal,
		// shear・特異な新親・表現不能な数値は拒否し、親とLocalを変更しない。
		KeepWorld
	};

	// 親・UUID・Script所有は、借用した値から書き換えない。
	template <class T>
	inline constexpr bool ReadOnlyComponent =
		std::is_same_v<T, Hierarchy> || std::is_same_v<T, PersistentId> || std::is_same_v<T, ScriptComponent>;
	// 保護されたcomponentは、非const Worldからも読み取り専用で借用する。
	template <class T> using BorrowedComponent = std::conditional_t<ReadOnlyComponent<T>, const T, T>;
	// 階層と永続IDの追加・削除は、専用の構造変更APIへ限定する。
	template <class T> inline constexpr bool ReservedComponent = std::is_same_v<T, Hierarchy> || std::is_same_v<T, PersistentId>;

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
		// 型を消した所有先から、実際のcomponentのデストラクタを呼ぶ基底。
		struct ComponentBase
		{
			// 登録時のtransaction番号。0はScene更新外の登録で、番号は再利用しない。
			std::uint64_t createdTransaction = 0;
			// 最初の値捕捉が成功したtransaction番号。実体を除去しても保持する。
			std::uint64_t capturedTransaction = 0;
			virtual ~ComponentBase() = default;
		};

		// componentを個別に所有し、別Entityの構造変更でもアドレスを保つ。
		template <ComponentType T> struct ComponentBox final : ComponentBase
		{
			template <class... Args>
			explicit ComponentBox(Args&&... args)
				: value(std::forward<Args>(args)...)
			{
			}

			T value;
		};

		// 一つのEntityが持つcomponentを、型ごとに所有する。
		struct EntityData
		{
			std::map<std::type_index, std::unique_ptr<ComponentBase>> components;
		};

		// Entityの世代と生存データを保持し、破棄後は再利用できる。
		struct Slot
		{
			// 世代が上限に達したスロットは0として再利用しない。
			std::uint64_t generation = 1;
			// rollback後も発行済み世代を再利用しない。生存中のgenerationと区別する。
			std::uint64_t lastIssuedGeneration = 0;
			// 生存中だけデータを所有する。空なら未使用または退役済み。
			std::unique_ptr<EntityData> data{};
		};

		// UUIDの16byte値から索引を引く。復元中もhash・比較で例外を送出しない。
		struct ObjectUuidHash
		{
			std::size_t operator()(const ObjectUuid& uuid) const noexcept;
		};
		// hashが衝突しても、保存UUIDの全byteが一致する場合だけ同じキーとする。
		struct ObjectUuidEqual
		{
			bool operator()(const ObjectUuid& first, const ObjectUuid& second) const noexcept;
		};
		// 生存中の永続UUIDだけを、World識別と世代を含むEntityへ対応付ける。
		using UuidIndex = std::unordered_map<ObjectUuid, Entity, ObjectUuidHash, ObjectUuidEqual>;

		// 構造変更の逆操作と、削除した実体・map nodeを成功確定まで所有する。
		struct UndoAction
		{
			// 記録順の構造変更に対応する逆操作。
			enum class Kind
			{
				Create,
				Destroy,
				Add,
				Remove
			};
			Kind kind;
			// Create/Destroyで変更したslot。
			std::uint32_t index = 0;
			// 削除したEntityを同じ世代で復元するための値。
			std::uint64_t generation = 0;
			// Add/Removeの対象。Entityが削除されてもjournal内で実体を保持する。
			EntityData* data = nullptr;
			// 追加したcomponentを逆操作で取り除く型。
			std::type_index type{typeid(void)};
			// 破棄を仮反映したEntityの、元の所有実体。
			std::unique_ptr<EntityData> destroyed;
			// 削除を仮反映したcomponentの、元のmap node。
			std::map<std::type_index, std::unique_ptr<ComponentBase>>::node_type removed;
			// Entity削除時のUUID node。rollbackで新規確保せず索引へ戻す。
			UuidIndex::node_type removedUuid;

			explicit UndoAction(Kind actionKind) noexcept
				: kind(actionKind)
			{
			}
		};

		// 一回のScene更新を戻すための構造・値の記録。World自体はコピーしない。
		struct Transaction
		{
			// 実体の登録・捕捉を定数時間で照合する、このWorld内の一意番号。
			std::uint64_t number = 0;
			std::size_t initialSlotCount = 0;
			// 更新開始前の空き順序。新規slotも確保なしで戻せる容量を用意する。
			std::vector<std::uint32_t> freeSlots;
			// 元の実体を借用する、値とScript内部状態の復元記録。
			std::vector<std::unique_ptr<RollbackState>> values;
			// 逆順で取り消す構造変更。削除した実体の所有もここへ移す。
			std::vector<UndoAction> actions;
		};

		// 構造変更の間だけ再入を拒否し、例外時も変更中の状態を解除する。
		class MutationGuard : private KT::Core::NonCopyable
		{
		public:
			explicit MutationGuard(World& world);
			~MutationGuard();

		private:
			// 変更中の状態を管理するWorldを、guardの寿命中だけ借用する。
			World& world_;
		};

		// 入れ子の列挙を数え、列挙中の構造変更を禁止する。
		class EnumerationGuard : private KT::Core::NonCopyable
		{
		public:
			explicit EnumerationGuard(const World& world);
			~EnumerationGuard();

		private:
			// 列挙数を管理するWorldを、guardの寿命中だけ借用する。
			const World& world_;
		};

	public:
		World();
		~World();
		[[nodiscard]] std::uint64_t Identity() const;
		[[nodiscard]] std::size_t Count() const;
		[[nodiscard]] bool IsAlive(Entity entity) const;
		// 生存中の保存UUIDを平均定数時間で検索する。無効・未登録はnullopt。
		[[nodiscard]] std::optional<Entity> FindByUuid(ObjectUuid uuid) const;
		// 生存Entityをスロット順にコピーする。component参照は保持しない。
		[[nodiscard]] std::vector<Entity> Entities() const;
		// 保存窓口が未対応の型を検出できるよう、型だけをコピーする。
		[[nodiscard]] std::vector<std::type_index> ComponentTypes(Entity entity) const;
		// 階層などの編集前に、列挙・構造変更中でないことを確認する。
		void RequireStructuralChange() const;
		[[nodiscard]] Entity CreateEntity();
		// 全子孫も子から順に破棄する。検証・確保失敗時は一体も変更しない。
		void DestroyEntity(Entity entity);
		// 全階層を検証する。KeepWorldの計算・確保失敗時はcomponentの有無も保持する。
		// 同じ親へのKeepWorldは何も変更しない。新しいLocalが必要ならcomponentを追加する。
		// 基底は行ごと、平行移動は成分ごとに相対誤差2e-5。平行移動は絶対誤差1e-5も許容する。
		// +1 scaleは誤差1e-6以内で1にそろえ、再構成したLocalとWorldの行列を検査する。
		void SetParent(Entity child, Entity parent = {}, ParentChangeMode mode = ParentChangeMode::KeepLocal);

		// 同じ型の追加はlogic_error。構築・確保に失敗した場合は登録しない。
		// コピー・ムーブできない型も、引数から直接構築する。
		template <ComponentType T, class... Args> T& AddComponent(Entity entity, Args&&... args)
		{
			static_assert(!ReservedComponent<T>, "HierarchyとPersistentIdの追加・削除には専用APIを使用してください。");

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
			ReserveUndo(1);
			components.emplace(type, std::move(componentBox));
			RecordAdded(*slots_[entity.index].data, type);

			return *value;
		}

		// componentを借用する。無効・失効・別World・型なしはnullptr。
		template <ComponentType T> [[nodiscard]] BorrowedComponent<T>* FindComponent(Entity entity)
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
		template <ComponentType T> [[nodiscard]] BorrowedComponent<T>& GetComponent(Entity entity)
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
			static_assert(!ReservedComponent<T>, "HierarchyとPersistentIdの追加・削除には専用APIを使用してください。");
			// 構造変更を開始し、対象Entityの生存を確認する。
			MutationGuard guard(*this);
			RequireAlive(entity);

			auto& components = slots_[entity.index].data->components;
			const auto componentIterator = components.find(std::type_index(typeid(T)));
			if (componentIterator == components.end())
			{
				throw std::out_of_range("要求したcomponentがありません。");
			}

			if (transaction_)
			{
				ReserveUndo(1);
				auto& action = transaction_->actions.emplace_back(UndoAction::Kind::Remove);
				action.data = slots_[entity.index].data.get();
				action.removed = components.extract(componentIterator);
			}
			else
			{
				components.erase(componentIterator);
			}
		}

		// 要求型をすべて持つEntityをスロット順に列挙する。型なしはコンパイル時に拒否する。
		// 値の編集・入れ子列挙は許可し、Create・Destroy・Add・Removeはlogic_error。
		// callbackの例外は伝播し、列挙状態を解除する。同じ型の重複指定は同じ参照を渡す。
		template <ComponentType... Ts, class Fn>
			requires(sizeof...(Ts) > 0 && std::invocable<Fn&, Entity, BorrowedComponent<Ts>&...>)
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
					std::invoke(callback, At(index), static_cast<BorrowedComponent<Ts>&>(*FindIn<Ts>(data))...);
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
		friend class WorldCommandBuffer;
		friend class Scene;
		void BeginTransaction();
		void CommitTransaction() noexcept;
		void RollbackTransaction() noexcept;
		void ReserveUndo(std::size_t count);
		// 捕捉callbackを呼ぶ前に、復元記録の容量を一定倍率で増やす。
		void ReserveValues(std::size_t count);
		// 生存数の最大値までbucketを確保し、rollback中のrehashを防ぐ。
		void ReserveUuidIndex(std::size_t count);
		void RecordAdded(EntityData& data, std::type_index type) noexcept;
		void BackupScript(ScriptBehaviour& script);

		template <ComponentType T> void BackupComponent(Entity entity)
		{
			RequireAlive(entity);
			auto& data = *slots_[entity.index].data;
			const auto iterator = data.components.find(typeid(T));
			if (iterator == data.components.end())
			{
				throw std::out_of_range("復元値を捕捉するComponentがありません。");
			}
			auto* component = iterator->second.get();

			// 新規実体は構造復元で破棄する。同じ実体の値は一回だけ捕捉する。
			if (!transaction_ || component->createdTransaction == transaction_->number ||
				component->capturedTransaction == transaction_->number)
			{
				return;
			}

			// SetParentの構造変更中も使う。捕捉callbackからの再入は拒否する。
			std::optional<MutationGuard> guard;
			if (!mutating_)
			{
				guard.emplace(*this);
			}
			ReserveValues(1);
			auto& value = static_cast<ComponentBox<T>&>(*component).value;
			std::unique_ptr<RollbackState> saved;

			// 値編集でだけ復元を要求する。非copy型の構造変更にはコピーを要求しない。
			if constexpr (requires(T& target) { { target.CaptureRollback() } -> std::same_as<std::unique_ptr<RollbackState>>; })
			{
				saved = value.CaptureRollback();
			}
			else if constexpr (std::is_copy_constructible_v<T> && std::is_nothrow_swappable_v<T>)
			{
				saved = std::make_unique<ValueRollback<T>>(value);
			}
			else
			{
				throw std::logic_error(std::string("Component値の編集にはCaptureRollbackの実装が必要です: ") + typeid(T).name());
			}
			if (!saved)
			{
				throw std::logic_error(std::string("ComponentのCaptureRollbackが復元記録を返しませんでした: ") + typeid(T).name());
			}
			transaction_->values.push_back(std::move(saved));
			component->capturedTransaction = transaction_->number;
		}
		Entity CreateSceneEntity(ObjectUuid uuid, std::string name);
		Entity PublishEntity(std::unique_ptr<EntityData> data);
		void DestroyEntity(Entity entity, std::vector<Entity>& destroyed);
		// 検証・確保を完了した部分木削除からだけ呼び、単一スロットを破棄する。
		void DestroySlot(Entity entity) noexcept;

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

		// 別WorldのEntityとの混同を防ぐ、再発行しない識別値。
		const std::uint64_t worldId_;
		// indexで参照する世代と所有データ。破棄しても並びを詰めない。
		std::vector<Slot> slots_{};
		// 再利用できるindexを保持し、末尾から定数時間で取得する。
		std::vector<std::uint32_t> freeSlots_{};
		// 永続UUIDの派生索引。生成・削除と同じjournalで復元し、列挙順には使わない。
		UuidIndex uuidIndex_;
		// reserve済みの要素数。削除で縮めず、復元時の最大生存数を収容する。
		std::size_t uuidIndexCapacity_ = 0;
		// データを所有している生存スロット数。
		std::size_t count_ = 0;
		// const列挙も含めた実行中の列挙数。
		mutable std::size_t enumerations_ = 0;
		// componentの構築・破棄を含む構造変更中は、読み取りも拒否する。
		bool mutating_ = false;
		// 捕捉に失敗した更新も消費する番号。rollback後も過去の印と一致させない。
		std::uint64_t lastTransactionNumber_ = 0;
		// Scene更新中だけ存在する。復元用領域は変更前に確保する。
		std::unique_ptr<Transaction> transaction_;
	};
}
