#include <World/World.h>
#include <World/Scene/TransformMath.h>
#include <World/Scene/Hierarchy.h>
#include <World/Scene/ScriptComponent.h>
#include <World/Systems/HierarchySystem.h>
#include <World/Systems/ActivationSystem.h>
#include <World/Systems/TransformSystem.h>
#include <Core/Math/Scalar.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <exception>
#include <limits>
#include <utility>

namespace KT::World
{
	namespace
	{
		// プロセス内で再発行しない。最大値は発行停止状態として予約する。
		std::atomic<std::uint64_t> nextWorldId{1};

		constexpr std::uint64_t NextGeneration(std::uint64_t generation) noexcept
		{
			return generation == 0 || generation == (std::numeric_limits<std::uint64_t>::max)() ? 0 : generation + 1;
		}

		std::uint32_t CheckedIndex(std::size_t index)
		{
			if (index >= Entity::InvalidIndex)
			{
				throw std::overflow_error("Entity indexが枯渇しました。");
			}

			return static_cast<std::uint32_t>(index);
		}

		std::uint64_t AcquireWorldId()
		{
			// 他Worldと重複しないIDを取得し、最大値は発行しない。
			auto candidateWorldId = nextWorldId.load(std::memory_order_relaxed);
			for (;;)
			{
				if (candidateWorldId == (std::numeric_limits<std::uint64_t>::max)())
				{
					throw std::overflow_error("World IDが枯渇しました。");
				}

				if (nextWorldId.compare_exchange_weak(candidateWorldId, candidateWorldId + 1, std::memory_order_relaxed))
				{
					return candidateWorldId;
				}
			}
		}
	}

	void World::EntityIndex::Prepare(std::uint32_t slot)
	{
		// denseと公開順の高水位容量を先に確保し、rollbackでの再追加も収容する。
		if (dense.size() >= Entity::InvalidIndex)
		{
			throw std::overflow_error("Entity所属索引の要素数が上限を超えます。");
		}
		const auto required = dense.size() + 1;
		const auto limit = (std::min)(dense.max_size(), ordered.max_size());
		if (required > limit)
		{
			throw std::overflow_error("Entity所属索引の配列容量が上限を超えます。");
		}
		const auto increment = (std::min)(dense.capacity() / 2 + 1, limit - dense.capacity());
		const auto capacity = (std::max)(required, dense.capacity() + increment);
		if (dense.capacity() < required)
		{
			dense.reserve(capacity);
		}
		if (ordered.capacity() < required)
		{
			ordered.reserve(capacity);
		}

		// 空き位置は明示し、同じslotの別世代を所属済みと見なさない。
		const auto sparseRequired = static_cast<std::size_t>(slot) + 1;
		if (sparseRequired > sparse.max_size())
		{
			throw std::overflow_error("Entity所属索引のslot範囲が上限を超えます。");
		}
		if (sparse.capacity() < sparseRequired)
		{
			const auto growth = (std::min)(sparse.capacity() / 2 + 1, sparse.max_size() - sparse.capacity());
			sparse.reserve((std::max)(sparseRequired, sparse.capacity() + growth));
		}
		if (sparse.size() < sparseRequired)
		{
			sparse.resize(sparseRequired, Entity::InvalidIndex);
		}
	}

	bool World::EntityIndex::Contains(Entity entity) const noexcept
	{
		if (!entity.IsValid() || entity.index >= sparse.size())
		{
			return false;
		}
		const auto position = sparse[entity.index];
		return position < dense.size() && dense[position] == entity;
	}

	void World::EntityIndex::Add(Entity entity) noexcept
	{
		// 正常な逆順復元では、元の所属数が過去の確保容量を超えることはない。
		if (entity.index >= sparse.size() || sparse[entity.index] != Entity::InvalidIndex ||
			dense.size() == dense.capacity() || ordered.capacity() < dense.size() + 1)
		{
			std::terminate();
		}
		sparse[entity.index] = static_cast<std::uint32_t>(dense.size());
		dense.push_back(entity);
		orderDirty = true;
	}

	void World::EntityIndex::Remove(Entity entity) noexcept
	{
		if (!Contains(entity))
		{
			std::terminate();
		}
		// 末尾を穴へ移し、移したEntityの逆引きも定数時間で修正する。
		const auto position = sparse[entity.index];
		const auto last = dense.back();
		dense[position] = last;
		sparse[last.index] = position;
		dense.pop_back();
		sparse[entity.index] = Entity::InvalidIndex;
		orderDirty = true;
	}

	const std::vector<Entity>& World::EntityIndex::Ordered() const noexcept
	{
		if (orderDirty)
		{
			// Prepareで確保した領域だけを使い、入れ子列挙も同じ完成順を借用する。
			ordered.assign(dense.begin(), dense.end());
			std::sort(ordered.begin(), ordered.end(),
				[](Entity first, Entity second)
				{
					return first.index < second.index;
				});
			orderDirty = false;
		}
		return ordered;
	}

	void World::PrepareComponentIndex(std::uint32_t slot, std::type_index type)
	{
		componentIndexes_[type].Prepare(slot);
	}

	const World::EntityIndex* World::FindComponentIndex(std::type_index type) const noexcept
	{
		const auto found = componentIndexes_.find(type);
		return found == componentIndexes_.end() ? nullptr : &found->second;
	}

	const World::EntityIndex* World::SmallestIndex(std::span<const EntityIndex* const> indexes) noexcept
	{
		const EntityIndex* smallest = nullptr;
		for (const auto* index : indexes)
		{
			if (!index)
			{
				return nullptr;
			}
			if (!smallest || index->dense.size() < smallest->dense.size())
			{
				smallest = index;
			}
		}
		return smallest;
	}

	World::World()
		: worldId_(AcquireWorldId())
	{
	}

	World::~World()
	{
		// 実行中のWorldを破棄してcallback/guardの参照を失効させない。
		if (mutating_ || enumerations_ != 0)
		{
			std::terminate();
		}

		// component破棄中も内部状態が有効な間に再入を拒否する。
		mutating_ = true;
		slots_.clear();
	}

	void World::BeginTransaction()
	{
		RequireStructuralChange();
		if (transaction_)
		{
			throw std::logic_error("Worldの更新transactionが既に開始されています。");
		}

		if (lastTransactionNumber_ == (std::numeric_limits<std::uint64_t>::max)())
		{
			throw std::overflow_error("Worldのtransaction番号が上限に達しました。");
		}

		// 番号は準備失敗時も消費する。再利用領域を確保してから空き順序を保存する。
		const auto transactionNumber = ++lastTransactionNumber_;
		auto& transaction = transactionStorage_;
		if (transaction.freeSlots.capacity() < slots_.size())
		{
			const auto increment = (std::min)(
				transaction.freeSlots.capacity() / 2 + 1, transaction.freeSlots.max_size() - transaction.freeSlots.capacity());
			transaction.freeSlots.reserve((std::max)(slots_.size(), transaction.freeSlots.capacity() + increment));
		}
		transaction.freeSlots.assign(freeSlots_.begin(), freeSlots_.end());
		transaction.number = transactionNumber;
		transaction.initialSlotCount = slots_.size();
		transaction.previousHierarchy = hierarchyCache_;
		// 新しい記録の準備が全て成功するまで、更新中として公開しない。
		transaction_ = &transaction;
	}

	void World::ReserveUndo(std::size_t count)
	{
		if (!transaction_)
		{
			return;
		}
		auto& actions = transaction_->actions;
		if (count > actions.max_size() - actions.size())
		{
			throw std::overflow_error("Worldの構造変更復元記録が上限を超えます。");
		}
		if (actions.capacity() < actions.size() + count)
		{
			const auto increment = (std::min)(actions.capacity() / 2 + 1, actions.max_size() - actions.capacity());
			actions.reserve((std::max)(actions.size() + count, actions.capacity() + increment));
		}
	}

	void World::NotifyCpuChange(EntityData& data, bool activation, bool transform)
	{
		if (lastCpuChangeVersion_ == (std::numeric_limits<std::uint64_t>::max)())
		{
			throw std::overflow_error("WorldのCPU入力の変更世代が上限に達しました。");
		}

		// 元のEntity実体の世代を一度だけ記録し、値編集や構造変更より前に確保する。
		if (transaction_ && data.capturedCpuTransaction != transaction_->number)
		{
			ReserveUndo(1);
			auto& action = transaction_->actions.emplace_back(UndoAction::Kind::CpuChanges);
			action.data = &data;
			action.previousCpuVersions = data.cpuVersions;
			data.capturedCpuTransaction = transaction_->number;
		}

		const auto version = ++lastCpuChangeVersion_;
		if (activation)
		{
			data.cpuVersions.activation = version;
		}
		if (transform)
		{
			data.cpuVersions.transform = version;
		}
	}

	CpuChangeState World::GetCpuChangeState(Entity entity) const
	{
		RequireReadable();
		RequireAlive(entity);
		const auto& data = *slots_[entity.index].data;
		CpuChangeState state{data.cpuVersions, cpuValueEditing_, cpuValueEditing_};
		if (const auto active = data.components.find(typeid(ActiveSelf)); active != data.components.end())
		{
			state.activationBorrowed = state.activationBorrowed || active->second->mutableBorrowed;
		}
		if (const auto transform = data.components.find(typeid(LocalTransform)); transform != data.components.end())
		{
			state.transformBorrowed = state.transformBorrowed || transform->second->mutableBorrowed;
		}
		return state;
	}

	void World::ReserveValues(std::size_t count)
	{
		auto& values = transaction_->values;
		if (count > values.max_size() - values.size())
		{
			throw std::overflow_error("Worldの値復元記録が上限を超えます。");
		}
		if (values.capacity() < values.size() + count)
		{
			const auto increment = (std::min)(values.capacity() / 2 + 1, values.max_size() - values.capacity());
			values.reserve((std::max)(values.size() + count, values.capacity() + increment));
		}
	}

	void World::ReserveUuidIndex(std::size_t count)
	{
		if (count > uuidIndex_.max_size() - uuidIndex_.size())
		{
			throw std::overflow_error("WorldのUUID索引が上限を超えます。");
		}
		const auto required = uuidIndex_.size() + count;
		if (uuidIndexCapacity_ < required)
		{
			const auto increment = (std::min)(uuidIndexCapacity_ / 2 + 1, uuidIndex_.max_size() - uuidIndexCapacity_);
			const auto capacity = (std::max)(required, uuidIndexCapacity_ + increment);
			uuidIndex_.reserve(capacity);
			uuidIndexCapacity_ = capacity;
		}
	}

	void World::RecordAdded(Entity entity, EntityData& data, std::type_index type) noexcept
	{
		componentIndexes_.find(type)->second.Add(entity);
		if (transaction_)
		{
			// 登録済みの実体に印を付ける。同型を追加し直した場合も元の実体と区別する。
			data.components.find(type)->second->createdTransaction = transaction_->number;
			auto& action = transaction_->actions.emplace_back(UndoAction::Kind::Add);
			action.data = &data;
			action.entity = entity;
			action.type = type;
		}
	}

	std::size_t World::ObjectUuidHash::operator()(const ObjectUuid& uuid) const noexcept
	{
		// 文字列化せず、16byte全体をFNV-1aで混ぜる。衝突時はUUID全体で比較する。
		std::uint64_t hash = 14695981039346656037ull;
		for (const auto byte : uuid.bytes)
		{
			hash ^= byte;
			hash *= 1099511628211ull;
		}
		return static_cast<std::size_t>(hash);
	}

	bool World::ObjectUuidEqual::operator()(const ObjectUuid& first, const ObjectUuid& second) const noexcept
	{
		return first.bytes == second.bytes;
	}

	void World::BackupScript(ScriptBehaviour& script)
	{
		MutationGuard guard(*this);
		ReserveValues(1);
		auto saved = script.CaptureRollback();
		if (!saved)
		{
			throw std::logic_error(std::string("ScriptのCaptureRollbackが復元記録を返しませんでした: ") + typeid(script).name());
		}
		transaction_->values.push_back(std::move(saved));
	}

	void World::CommitTransaction() noexcept
	{
		mutating_ = true;
		// 復元記録の借用先を保持したまま記録を破棄し、削除実体は操作順で解放する。
		transaction_->values.clear();
		for (auto& action : transaction_->actions)
		{
			action.destroyed.reset();
			action.removed = {};
			action.removedUuid = {};
		}
		// 所有・借用と更新情報だけを消し、配列容量は次回の復元記録へ再利用する。
		transaction_->actions.clear();
		transaction_->freeSlots.clear();
		transaction_->previousHierarchy.reset();
		transaction_->number = 0;
		transaction_->initialSlotCount = 0;
		transaction_ = nullptr;
		mutating_ = false;
	}

	void World::RollbackTransaction() noexcept
	{
		if (!transaction_)
		{
			return;
		}
		mutating_ = true;

		// 削除した実体も生存している間に値を戻し、構造を逆順で復元する。
		for (auto iterator = transaction_->values.rbegin(); iterator != transaction_->values.rend(); ++iterator)
		{
			(*iterator)->Restore();
		}
		transaction_->values.clear();
		for (auto iterator = transaction_->actions.rbegin(); iterator != transaction_->actions.rend(); ++iterator)
		{
			auto& action = *iterator;
			switch (action.kind)
			{
			case UndoAction::Kind::Create:
				for (const auto& [type, component] : slots_[action.index].data->components)
				{
					(void)component;
					componentIndexes_.find(type)->second.Remove(At(action.index));
				}
				liveEntities_.Remove(At(action.index));
				if (const auto* identity = FindIn<PersistentId>(*slots_[action.index].data))
				{
					uuidIndex_.erase(identity->value);
				}
				slots_[action.index].data.reset();
				slots_[action.index].generation = NextGeneration(slots_[action.index].lastIssuedGeneration);
				--count_;
				break;
			case UndoAction::Kind::Destroy:
				slots_[action.index].data = std::move(action.destroyed);
				slots_[action.index].generation = action.generation;
				liveEntities_.Add(At(action.index));
				for (const auto& [type, component] : slots_[action.index].data->components)
				{
					(void)component;
					componentIndexes_.find(type)->second.Add(At(action.index));
				}
				++count_;
				if (!action.removedUuid.empty())
				{
					// nodeと過去の最大生存数までのbucketを保持しているため、復元で確保しない。
					const auto restored = uuidIndex_.insert(std::move(action.removedUuid));
					// 逆順の復元では同じUUIDは残らない。重複は内部不変条件の破損として止める。
					if (!restored.inserted)
					{
						std::terminate();
					}
				}
				break;
			case UndoAction::Kind::Add:
				componentIndexes_.find(action.type)->second.Remove(action.entity);
				action.data->components.erase(action.type);
				break;
			case UndoAction::Kind::Remove:
				componentIndexes_.find(action.removed.key())->second.Add(action.entity);
				action.data->components.insert(std::move(action.removed));
				break;
			case UndoAction::Kind::CpuChanges:
				action.data->cpuVersions = action.previousCpuVersions;
				break;
			}
		}

		// 発行した世代は消費したままにし、失敗中のhandleが再び有効になるのを防ぐ。
		auto& restoredFreeSlots = transaction_->freeSlots;
		std::erase_if(restoredFreeSlots, [this](std::uint32_t index)
			{
				return slots_[index].generation == 0;
			});
		for (std::size_t index = transaction_->initialSlotCount; index < slots_.size(); ++index)
		{
			if (slots_[index].generation != 0)
			{
				restoredFreeSlots.push_back(static_cast<std::uint32_t>(index));
			}
		}
		freeSlots_.swap(restoredFreeSlots);
		// 元の階層とEntity世代を戻してから、更新前の索引を確保なしで再公開する。
		hierarchyCache_ = std::move(transaction_->previousHierarchy);
		// 所有・借用と更新情報だけを消し、配列容量は次回の復元記録へ再利用する。
		transaction_->actions.clear();
		transaction_->freeSlots.clear();
		transaction_->previousHierarchy.reset();
		transaction_->number = 0;
		transaction_->initialSlotCount = 0;
		transaction_ = nullptr;
		mutating_ = false;
	}

	void World::RequireReadable() const
	{
		if (mutating_)
		{
			throw std::logic_error("componentの構築/破棄または構造変更中はWorldへ再入できません。");
		}
	}

	World::MutationGuard::MutationGuard(World& world)
		: world_(world)
	{
		world_.RequireStructuralChange();
		world_.mutating_ = true;
	}

	World::MutationGuard::~MutationGuard()
	{
		world_.mutating_ = false;
	}

	World::EnumerationGuard::EnumerationGuard(const World& world)
		: world_(world)
	{
		world_.RequireReadable();
		if (world_.enumerations_ == (std::numeric_limits<std::size_t>::max)())
		{
			throw std::overflow_error("列挙の入れ子数が上限に達しました。");
		}

		++world_.enumerations_;
	}

	World::EnumerationGuard::~EnumerationGuard()
	{
		--world_.enumerations_;
	}

	std::uint64_t World::Identity() const
	{
		RequireReadable();
		return worldId_;
	}

	std::size_t World::Count() const
	{
		RequireReadable();
		return count_;
	}

	bool World::IsAliveUnchecked(Entity entity) const noexcept
	{
		return entity.IsValid() && entity.worldId == worldId_ && entity.index < slots_.size() && slots_[entity.index].data &&
			slots_[entity.index].generation == entity.generation;
	}

	bool World::IsAlive(Entity entity) const
	{
		RequireReadable();
		return IsAliveUnchecked(entity);
	}

	std::optional<Entity> World::FindByUuid(ObjectUuid uuid) const
	{
		RequireReadable();
		const auto entry = uuidIndex_.find(uuid);
		if (entry == uuidIndex_.end() || !IsAliveUnchecked(entry->second))
		{
			return std::nullopt;
		}
		return entry->second;
	}

	std::vector<Entity> World::Entities() const
	{
		RequireReadable();

		// 空きslotを走査せず、所属変更後だけ整列した生存handleをコピーする。
		return liveEntities_.Ordered();
	}

	void World::RequireStructuralChange() const
	{
		RequireReadable();
		if (enumerations_ != 0)
		{
			throw std::logic_error("列挙中はWorldの構造を変更できません。");
		}
	}

	std::vector<std::type_index> World::ComponentTypes(Entity entity) const
	{
		RequireReadable();
		RequireAlive(entity);

		// component実体を借用せず、保存窓口に型の一覧だけを渡す。
		const auto& components = slots_[entity.index].data->components;
		std::vector<std::type_index> types;
		types.reserve(components.size());
		for (const auto& [type, component] : components)
		{
			(void)component;
			types.push_back(type);
		}
		return types;
	}

	void World::RequireAlive(Entity entity) const
	{
		if (!IsAliveUnchecked(entity))
		{
			throw std::invalid_argument("Entityが無効/失効済み、または別Worldのものです。");
		}
	}

	Entity World::At(std::size_t index) const noexcept
	{
		return {worldId_, static_cast<std::uint32_t>(index), slots_[index].generation};
	}

	Entity World::CreateEntity()
	{
		MutationGuard guard(*this);
		return PublishEntity(std::make_unique<EntityData>());
	}

	Entity World::CreateSceneEntity(ObjectUuid uuid, std::string name)
	{
		MutationGuard guard(*this);
		if (!uuid.IsValid())
		{
			throw std::invalid_argument("生成するSceneオブジェクトのUUIDが空です。");
		}

		// 同じWorld内で永続UUIDが重複しないことを、公開前に確認する。
		if (uuidIndex_.contains(uuid))
		{
			throw std::invalid_argument("生成するSceneオブジェクトのUUIDが同じWorld内で重複しています。");
		}

		// 必須componentの構築と登録を済ませてから、Entityを公開する。
		auto data = std::make_unique<EntityData>();
		data->components.emplace(typeid(PersistentId), std::make_unique<ComponentBox<PersistentId>>(uuid));
		data->components.emplace(typeid(Name), std::make_unique<ComponentBox<Name>>(std::move(name)));

		// Entityを公開する前に索引nodeとbucketを確保し、公開失敗なら仮nodeを除く。
		ReserveUuidIndex(1);
		const auto [entry, inserted] = uuidIndex_.emplace(uuid, Entity{});
		if (!inserted)
		{
			throw std::logic_error("生成前に検査したUUIDを索引へ登録できませんでした。");
		}

		try
		{
			const auto entity = PublishEntity(std::move(data));
			entry->second = entity;
			return entity;
		}
		catch (...)
		{
			uuidIndex_.erase(entry);
			throw;
		}
	}

	void World::HierarchyBatch::SetParent(std::uint32_t child, std::uint32_t parent) noexcept
	{
		auto& childLinks = links[child];
		if (childLinks.parent == parent)
		{
			return;
		}

		orderDirty = true;

		// 現在の親と兄弟から外す。根は兄弟リストを持たない。
		if (childLinks.parent != Entity::InvalidIndex)
		{
			if (childLinks.previousSibling == Entity::InvalidIndex)
			{
				links[childLinks.parent].firstChild = childLinks.nextSibling;
			}
			else
			{
				links[childLinks.previousSibling].nextSibling = childLinks.nextSibling;
			}
			if (childLinks.nextSibling != Entity::InvalidIndex)
			{
				links[childLinks.nextSibling].previousSibling = childLinks.previousSibling;
			}
		}

		// 新親の子リストへ定数時間で挿入し、列挙順の整列は削除対象の収集時に行う。
		childLinks.parent = parent;
		childLinks.previousSibling = Entity::InvalidIndex;
		childLinks.nextSibling = Entity::InvalidIndex;
		if (parent != Entity::InvalidIndex)
		{
			childLinks.nextSibling = links[parent].firstChild;
			if (childLinks.nextSibling != Entity::InvalidIndex)
			{
				links[childLinks.nextSibling].previousSibling = child;
			}
			links[parent].firstChild = child;
		}
	}

	void World::HierarchyBatch::Remove(std::uint32_t entity) noexcept
	{
		// 子を先に削除した対象だけを外し、再利用するslotにリンクを残さない。
		SetParent(entity, Entity::InvalidIndex);
		links[entity] = {};
		orderDirty = true;
	}

	void World::BeginHierarchyBatch()
	{
		RequireStructuralChange();
		if (hierarchyBatch_)
		{
			throw std::logic_error("Worldの階層予約batchが既に開始されています。");
		}

		// slot位置の領域は残し、生存Entityの所属からだけ作業リンクを作る。
		auto batch = std::make_unique<HierarchyBatch>();
		batch->links.resize(slots_.size());
		const auto& entities = liveEntities_.Ordered();
		for (const auto entity : entities)
		{
			batch->links[entity.index].alive = true;
		}
		for (const auto entity : entities)
		{
			const auto* hierarchy = FindIn<Hierarchy>(*slots_[entity.index].data);
			if (hierarchy && hierarchy->parent != Entity{})
			{
				RequireAlive(hierarchy->parent);
				batch->SetParent(entity.index, hierarchy->parent.index);
			}
		}

		// 根からの到達数で循環を検査する。以後の親変更は祖先列だけを検査する。
		std::vector<std::uint32_t> reached;
		reached.reserve(count_);
		for (const auto entity : entities)
		{
			if (batch->links[entity.index].parent == Entity::InvalidIndex)
			{
				reached.push_back(entity.index);
			}
		}
		for (std::size_t index = 0; index < reached.size(); ++index)
		{
			for (auto child = batch->links[reached[index]].firstChild; child != Entity::InvalidIndex;
				child = batch->links[child].nextSibling)
			{
				reached.push_back(child);
			}
		}
		if (reached.size() != count_)
		{
			throw std::invalid_argument("階層予約batchの開始時の親子関係が循環しています。");
		}
		batch->parentFirst = std::move(reached);
		batch->orderDirty = false;
		hierarchyBatch_ = std::move(batch);
	}

	void World::EndHierarchyBatch() noexcept
	{
		// 親の正本とjournalはWorldに残し、作業索引だけを破棄する。
		hierarchyBatch_.reset();
	}

	void World::ComputeBatchWorld(Entity child, Entity parent, KT::Core::Math::Matrix4& oldWorld,
		KT::Core::Math::Matrix4& parentWorld) const
	{
		// 構造変更後だけ作業順を再構築し、行列の領域もbatch内で再利用する。
		auto& batch = *hierarchyBatch_;
		if (batch.orderDirty)
		{
			batch.parentFirst.clear();
			batch.parentFirst.reserve(count_);
			for (const auto entity : liveEntities_.Ordered())
			{
				if (batch.links[entity.index].parent == Entity::InvalidIndex)
				{
					batch.parentFirst.push_back(entity.index);
				}
			}
			for (std::size_t index = 0; index < batch.parentFirst.size(); ++index)
			{
				for (auto descendant = batch.links[batch.parentFirst[index]].firstChild; descendant != Entity::InvalidIndex;
					descendant = batch.links[descendant].nextSibling)
				{
					batch.parentFirst.push_back(descendant);
				}
			}
			batch.orderDirty = false;
		}
		batch.transforms.resize(batch.links.size());
		for (auto entityIndex : batch.parentFirst)
		{
			const auto entity = At(entityIndex);
			const auto& links = batch.links[entityIndex];
			const auto changes = GetCpuChangeState(entity);
			auto& cached = batch.transforms[entityIndex];
			cached.changed = !cached.valid || cached.entity != entity || cached.parent != links.parent ||
				cached.version != changes.versions.transform || changes.transformBorrowed ||
				(links.parent != Entity::InvalidIndex && batch.transforms[links.parent].changed);
			if (cached.changed)
			{
				const auto* transform = FindIn<LocalTransform>(*slots_[entityIndex].data);
				const auto local = transform ? *transform : LocalTransform{};
				const auto matrix = KT::Core::Math::LocalMatrix(local.position, local.rotation, local.scale);
				cached.matrix = links.parent == Entity::InvalidIndex ? matrix :
					KT::Core::Math::Multiply(matrix, batch.transforms[links.parent].matrix);
				cached.entity = entity;
				cached.parent = links.parent;
				cached.version = changes.versions.transform;
				cached.valid = true;
			}
		}
		// 対象外も初回・変更・可変借用時には検証し、全件成功後に結果を返す。
		oldWorld = batch.transforms[child.index].matrix;
		if (parent != Entity{})
		{
			parentWorld = batch.transforms[parent.index].matrix;
		}
	}

	std::shared_ptr<const HierarchySnapshot> World::GetHierarchy() const
	{
		RequireReadable();
		if (!hierarchyCache_)
		{
			// 親だけをslot順で捕捉し、値編集に依存しない派生索引を作る。
			std::vector<HierarchyInput> inputs;
			inputs.reserve(count_);
			for (const auto entity : liveEntities_.Ordered())
			{
				const auto* hierarchy = FindIn<Hierarchy>(*slots_[entity.index].data);
				inputs.push_back({entity, hierarchy ? hierarchy->parent : Entity{}});
			}

			// 検証と確保が全て成功した値だけを公開し、失敗時は未構築のまま保つ。
			auto snapshot = std::make_shared<const HierarchySnapshot>(HierarchySystem::Build(inputs));
			hierarchyCache_ = std::move(snapshot);
		}
		return hierarchyCache_;
	}

	Entity World::PublishEntity(std::unique_ptr<EntityData> data)
	{
		// 空きslotの取得は定数時間。確保失敗時はfree listを消費しない。
		const auto index = freeSlots_.empty() ? CheckedIndex(slots_.size()) : freeSlots_.back();
		ReserveUndo(1);
		// 生存・型別索引の確保が全て成功するまで、slotと所有実体を公開しない。
		liveEntities_.Prepare(index);
		for (const auto& [type, component] : data->components)
		{
			(void)component;
			PrepareComponentIndex(index, type);
		}
		// 作業索引の追加領域も、slotやfree listを変更する前に確保する。
		if (hierarchyBatch_ && index >= hierarchyBatch_->links.size())
		{
			hierarchyBatch_->links.resize(static_cast<std::size_t>(index) + 1);
		}
		if (freeSlots_.empty())
		{
			// 各slotを破棄時に確保なしで戻せる容量を、作成時に用意する。
			const auto required = slots_.size() + 1;
			if (freeSlots_.capacity() < required)
			{
				const auto growth = freeSlots_.capacity() + freeSlots_.capacity() / 2 + 1;
				freeSlots_.reserve((std::max)(required, growth));
			}
			if (transaction_)
			{
				auto& restoredFreeSlots = transaction_->freeSlots;
				if (restoredFreeSlots.capacity() < required)
				{
					const auto increment = (std::min)(
						restoredFreeSlots.capacity() / 2 + 1, restoredFreeSlots.max_size() - restoredFreeSlots.capacity());
					restoredFreeSlots.reserve((std::max)(required, restoredFreeSlots.capacity() + increment));
				}
			}
			slots_.emplace_back();
		}
		else
		{
			freeSlots_.pop_back();
		}

		if (transaction_)
		{
			// Entity生成でまとめて登録するComponentも、今回作った実体として扱う。
			for (auto& [type, component] : data->components)
			{
				(void)type;
				component->createdTransaction = transaction_->number;
			}
			auto& action = transaction_->actions.emplace_back(UndoAction::Kind::Create);
			action.index = index;
		}
		slots_[index].data = std::move(data);
		slots_[index].lastIssuedGeneration = slots_[index].generation;
		liveEntities_.Add(At(index));
		for (const auto& [type, component] : slots_[index].data->components)
		{
			(void)component;
			componentIndexes_.find(type)->second.Add(At(index));
		}
		++count_;
		if (hierarchyBatch_)
		{
			hierarchyBatch_->links[index] = {};
			hierarchyBatch_->links[index].alive = true;
			hierarchyBatch_->orderDirty = true;
		}
		hierarchyCache_.reset();
		return At(index);
	}

	void World::SetParent(Entity child, Entity parent, ParentChangeMode mode)
	{
		RequireStructuralChange();
		RequireAlive(child);
		if (parent != Entity{})
		{
			RequireAlive(parent);
		}
		if (mode != ParentChangeMode::KeepLocal && mode != ParentChangeMode::KeepWorld)
		{
			throw std::invalid_argument("親変更の変換維持方法が不正です。");
		}

		// 検証済みの親索引で新親の祖先だけを辿り、循環を作る変更を拒否する。
		std::shared_ptr<const HierarchySnapshot> topology;
		if (!hierarchyBatch_)
		{
			topology = GetHierarchy();
		}
		auto& data = *slots_[child.index].data;
		auto* hierarchy = FindIn<Hierarchy>(data);
		const auto oldParent = hierarchy ? hierarchy->parent : Entity{};
		for (auto ancestor = parent; ancestor != Entity{};)
		{
			if (ancestor == child)
			{
				throw std::invalid_argument("親変更によって階層の親子関係が循環します。");
			}
			if (hierarchyBatch_)
			{
				const auto parentIndex = hierarchyBatch_->links[ancestor.index].parent;
				ancestor = parentIndex == Entity::InvalidIndex ? Entity{} : At(parentIndex);
			}
			else
			{
				const auto parentIndex = topology->nodes[topology->FindNode(ancestor)].parent;
				ancestor = parentIndex == NoParent ? Entity{} : topology->nodes[parentIndex].entity;
			}
		}

		if (mode == ParentChangeMode::KeepWorld)
		{
			if (oldParent == parent)
			{
				return;
			}

			// 現在のWorldを再計算する。予約より前のsnapshotを使わず、先行操作を含める。
			KT::Core::Math::Matrix4 oldWorld;
			KT::Core::Math::Matrix4 parentWorld;
			if (hierarchyBatch_)
			{
				ComputeBatchWorld(child, parent, oldWorld, parentWorld);
			}
			else
			{
				SceneUpdateContext context(*this, std::nullopt, {});
				HierarchySystem{}.Update(context);
				ActivationSystem{}.Update(context);
				TransformSystem{}.Update(context);
				const auto& transforms = context.GetTransforms();
				oldWorld = transforms[topology->FindNode(child)].transform.matrix;
				if (parent != Entity{})
				{
					parentWorld = transforms[topology->FindNode(parent)].transform.matrix;
				}
			}

			const auto localMatrix = KT::Core::Math::Multiply(oldWorld, KT::Core::Math::Inverse(parentWorld));
			const auto local = DecomposeLocalTransform(localMatrix);
			const auto restoredWorld = KT::Core::Math::Multiply(
				KT::Core::Math::LocalMatrix(local.position, local.rotation, local.scale), parentWorld);
			if (!TransformNear(oldWorld, restoredWorld))
			{
				throw std::invalid_argument("KeepWorldの親変更後のWorld行列を許容誤差内に維持できません。");
			}

			// 不足componentとmap nodeを別の表で確保し、二つの追加の途中失敗を防ぐ。
			MutationGuard guard(*this);
			std::map<std::type_index, std::unique_ptr<ComponentBase>> additions;
			if (!hierarchy)
			{
				additions.emplace(typeid(Hierarchy), std::make_unique<ComponentBox<Hierarchy>>(parent));
			}
			auto* transform = FindIn<LocalTransform>(data);
			if (!transform)
			{
				additions.emplace(typeid(LocalTransform), std::make_unique<ComponentBox<LocalTransform>>(local));
			}

			// 二型の所属領域も全て準備し、後半の確保失敗で片方だけ公開しない。
			for (const auto& [type, component] : additions)
			{
				(void)component;
				PrepareComponentIndex(child.index, type);
			}

			// type_indexの比較は例外を送出せず、node移動・値代入は確保を伴わない。
			ReserveUndo(additions.size());
			if (hierarchy)
			{
				BackupComponent<Hierarchy>(child);
			}
			if (transform)
			{
				BackupComponent<LocalTransform>(child);
			}
			NotifyCpuChange(data, true, true);
			// 世代の復元記録追加後に、Component追加分の空きを確認する。
			ReserveUndo(additions.size());
			data.components.merge(additions);
			if (!hierarchy)
			{
				RecordAdded(child, data, typeid(Hierarchy));
			}
			if (!transform)
			{
				RecordAdded(child, data, typeid(LocalTransform));
			}
			if (hierarchy)
			{
				hierarchy->parent = parent;
			}
			if (transform)
			{
				*transform = local;
			}
			if (hierarchyBatch_)
			{
				hierarchyBatch_->SetParent(child.index, parent.index);
			}
			hierarchyCache_.reset();
			return;
		}

		// KeepLocalは既存のローカル値とcomponent欠落を保つ。
		MutationGuard guard(*this);
		if (oldParent != parent)
		{
			NotifyCpuChange(data, true, true);
		}
		if (hierarchy)
		{
			BackupComponent<Hierarchy>(child);
			hierarchy->parent = parent;
		}
		else
		{
			PrepareComponentIndex(child.index, typeid(Hierarchy));
			ReserveUndo(1);
			data.components.emplace(typeid(Hierarchy), std::make_unique<ComponentBox<Hierarchy>>(parent));
			RecordAdded(child, data, typeid(Hierarchy));
		}
		if (oldParent != parent)
		{
			if (hierarchyBatch_)
			{
				hierarchyBatch_->SetParent(child.index, parent.index);
			}
			hierarchyCache_.reset();
		}
	}

	void World::DestroyEntity(Entity entity)
	{
		std::vector<Entity> destroyed;
		DestroyEntity(entity, destroyed);
	}

	std::size_t World::DestroyedEntityHash::operator()(Entity entity) const noexcept
	{
		// 全識別値を混ぜる。衝突時もEntity全体の等値比較で照合する。
		std::uint64_t hash = entity.worldId;
		hash ^= static_cast<std::uint64_t>(entity.index) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
		hash ^= entity.generation + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
		return static_cast<std::size_t>(hash);
	}

	void World::DestroyEntity(Entity entity, std::vector<Entity>& destroyed, DestroyedEntityIndex* destroyedIndex)
	{
		RequireStructuralChange();
		RequireAlive(entity);

		// 全階層を検証し、削除対象の全子孫を親から順に集める。
		std::vector<Entity> subtree;
		subtree.push_back(entity);
		if (hierarchyBatch_)
		{
			for (std::size_t index = 0; index < subtree.size(); ++index)
			{
				const auto childrenBegin = subtree.size();
				for (auto child = hierarchyBatch_->links[subtree[index].index].firstChild; child != Entity::InvalidIndex;
					child = hierarchyBatch_->links[child].nextSibling)
				{
					subtree.push_back(At(child));
				}
				// 作業リンクの挿入順に依存せず、既存と同じslot順で兄弟を収集する。
				if (childrenBegin > static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)()))
				{
					throw std::overflow_error("部分木の兄弟列の位置がiterator差分型の上限を超えます。");
				}
				std::sort(subtree.begin() + static_cast<std::ptrdiff_t>(childrenBegin), subtree.end(),
					[](Entity first, Entity second) { return first.index < second.index; });
			}
		}
		else
		{
			const auto hierarchy = GetHierarchy();
			for (std::size_t index = 0; index < subtree.size(); ++index)
			{
				for (const auto child : hierarchy->nodes[hierarchy->FindNode(subtree[index])].children)
				{
					subtree.push_back(hierarchy->nodes[child].entity);
				}
			}
		}

		// 対象記録も破棄前に確保し、commit中に例外を発生させない。
		if (subtree.size() > destroyed.max_size() - destroyed.size())
		{
			throw std::overflow_error("部分木削除のEntity記録数がコンテナーの上限を超えます。");
		}

		const auto required = destroyed.size() + subtree.size();
		if (destroyed.capacity() < required)
		{
			const auto increment = (std::min)(destroyed.capacity() / 2 + 1, destroyed.max_size() - destroyed.capacity());
			destroyed.reserve((std::max)(required, destroyed.capacity() + increment));
		}
		ReserveUndo(subtree.size());

		// hash nodeも破棄前に用意する。生存する部分木のhandleは過去の削除履歴と重ならない。
		if (destroyedIndex)
		{
			auto& index = *destroyedIndex;
			if (subtree.size() > index.entities.max_size() - index.entities.size())
			{
				throw std::overflow_error("削除済みEntityの索引要素数が上限を超えます。");
			}
			const auto indexedRequired = index.entities.size() + subtree.size();
			if (index.capacity < indexedRequired)
			{
				const auto increment = (std::min)(index.capacity / 2 + 1, index.entities.max_size() - index.capacity);
				const auto capacity = (std::max)(indexedRequired, index.capacity + increment);
				index.entities.reserve(capacity);
				index.capacity = capacity;
			}
		}

		// 対象を逆順に破棄し、子のcomponentを親より先に破棄する。
		MutationGuard guard(*this);
		if (destroyedIndex)
		{
			std::size_t insertedCount = 0;
			try
			{
				for (const auto target : subtree)
				{
					if (!destroyedIndex->entities.insert(target).second)
					{
						throw std::logic_error("生存する部分木のEntityが削除済み索引に含まれています。");
					}
					++insertedCount;
				}
			}
			catch (...)
			{
				// 登録途中の確保失敗では仮nodeだけを除き、Worldと以前の履歴を保つ。
				for (std::size_t index = 0; index < insertedCount; ++index)
				{
					destroyedIndex->entities.erase(subtree[index]);
				}
				throw;
			}
		}

		for (auto iterator = subtree.rbegin(); iterator != subtree.rend(); ++iterator)
		{
			const auto target = *iterator;
			destroyed.push_back(target);
			DestroySlot(target);
		}
	}

	void World::DestroySlot(Entity entity) noexcept
	{
		// Entityを失効させる。更新中の実体解放は成功確定まで延期する。
		auto& slot = slots_[entity.index];
		for (const auto& [type, component] : slot.data->components)
		{
			(void)component;
			componentIndexes_.find(type)->second.Remove(entity);
		}
		liveEntities_.Remove(entity);
		const auto* identity = FindIn<PersistentId>(*slot.data);
		if (transaction_)
		{
			auto& action = transaction_->actions.emplace_back(UndoAction::Kind::Destroy);
			action.index = entity.index;
			action.generation = slot.generation;
			if (identity)
			{
				// 同じUUIDを後続予約で再生成できるよう、索引から外してnodeを保持する。
				action.removedUuid = uuidIndex_.extract(identity->value);
			}
			action.destroyed = std::move(slot.data);
		}
		else
		{
			if (identity)
			{
				uuidIndex_.erase(identity->value);
			}
			slot.data.reset();
		}

		// 上限世代のslotは0で退役。古いhandleと一致する世代へ循環させない。
		slot.generation = NextGeneration(slot.lastIssuedGeneration);
		--count_;
		if (hierarchyBatch_)
		{
			hierarchyBatch_->Remove(entity.index);
		}
		hierarchyCache_.reset();
		if (slot.generation != 0)
		{
			freeSlots_.push_back(entity.index);
		}
	}
}
