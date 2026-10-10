#include <World/World.h>
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

		// 行ごとの大きさで誤差を測り、大きな平行移動で回転の誤差を隠さない。
		bool TransformNear(const KT::Core::Math::Matrix4& first, const KT::Core::Math::Matrix4& second)
		{
			for (std::size_t row = 0; row < 4; ++row)
			{
				double magnitude = 0;
				for (std::size_t column = 0; column < 3; ++column)
				{
					magnitude = (std::max)({magnitude, std::abs(double(first(row, column))), std::abs(double(second(row, column)))});
				}

				for (std::size_t column = 0; column < 3; ++column)
				{
					// 平行移動は各成分で比較し、原点への相殺誤差には絶対許容値を使う。
					const auto translationMagnitude = (std::max)(std::abs(double(first(row, column))), std::abs(double(second(row, column))));
					const auto tolerance = row == 3 ? (std::max)(1.0e-5, 2.0e-5 * translationMagnitude) : 2.0e-5 * magnitude;
					if (std::abs(double(first(row, column)) - second(row, column)) > tolerance)
					{
						return false;
					}
				}
			}
			return true;
		}

		// S*R*Tの行基底を分解する。失われた回転軸は補完し、shearは近似しない。
		LocalTransform DecomposeLocalTransform(const KT::Core::Math::Matrix4& matrix)
		{
			using KT::Core::Math::Detail::CheckedFloat;
			if (!KT::Core::Math::IsAffine(matrix))
			{
				throw std::invalid_argument("KeepWorldの新Local行列が有限のaffine行列ではありません。");
			}

			std::array<std::array<double, 3>, 3> axes{};
			std::array<double, 3> scales{};
			std::size_t presentCount = 0;
			std::size_t presentAxis = 0;
			std::size_t missingAxis = 0;
			for (std::size_t row = 0; row < 3; ++row)
			{
				scales[row] = std::hypot(double(matrix(row, 0)), double(matrix(row, 1)), double(matrix(row, 2)));
				if (scales[row] == 0)
				{
					missingAxis = row;
					continue;
				}

				++presentCount;
				presentAxis = row;
				for (std::size_t column = 0; column < 3; ++column)
				{
					axes[row][column] = matrix(row, column) / scales[row];
				}
			}

			const auto cross = [](const auto& first, const auto& second) -> std::array<double, 3>
			{
				return {first[1] * second[2] - first[2] * second[1], first[2] * second[0] - first[0] * second[2],
					first[0] * second[1] - first[1] * second[0]};
			};
			const auto dot = [](const auto& first, const auto& second)
			{
				return first[0] * second[0] + first[1] * second[1] + first[2] * second[2];
			};

			// 非zeroの行が直交しない場合、LocalTransformでは表現できない。
			for (std::size_t first = 0; first < 3; ++first)
			{
				for (std::size_t second = first + 1; second < 3; ++second)
				{
					if (scales[first] != 0 && scales[second] != 0 && std::abs(dot(axes[first], axes[second])) > 1.0e-5)
					{
						throw std::invalid_argument("KeepWorldの新Local行列にshearがあり、位置・回転・scaleで表現できません。");
					}
				}
			}

			// zero scaleの行を正の行列式の直交基底で補完する。残る行列の値は変えない。
			if (presentCount == 0)
			{
				axes = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
			}
			else if (presentCount == 1)
			{
				const auto& known = axes[presentAxis];
				const auto helperAxis = static_cast<std::size_t>(std::min_element(known.begin(), known.end(),
					[](double first, double second)
					{
						return std::abs(first) < std::abs(second);
					}) - known.begin());
				std::array<double, 3> helper{};
				helper[helperAxis] = 1;
				const auto nextAxis = (presentAxis + 1) % 3;
				axes[nextAxis] = cross(helper, known);
				const auto length = std::hypot(axes[nextAxis][0], axes[nextAxis][1], axes[nextAxis][2]);
				for (auto& value : axes[nextAxis])
				{
					value /= length;
				}
				axes[(presentAxis + 2) % 3] = cross(known, axes[nextAxis]);
			}
			else if (presentCount == 2)
			{
				axes[missingAxis] = cross(axes[(missingAxis + 1) % 3], axes[(missingAxis + 2) % 3]);
			}
			else if (dot(axes[0], cross(axes[1], axes[2])) < 0)
			{
				// 反転は最大scale軸の符号へ移し、Quaternionには純粋な回転を渡す。
				const auto reflectedAxis = static_cast<std::size_t>(std::max_element(scales.begin(), scales.end()) - scales.begin());
				scales[reflectedAxis] = -scales[reflectedAxis];
				for (auto& value : axes[reflectedAxis])
				{
					value = -value;
				}
			}

			// row-vectorの回転行列から、最大成分を使ってQuaternionを安定に求める。
			std::array<double, 4> rotation{};
			const auto trace = axes[0][0] + axes[1][1] + axes[2][2];
			if (trace > 0)
			{
				const auto divisor = 2 * std::sqrt(trace + 1);
				rotation = {(axes[1][2] - axes[2][1]) / divisor, (axes[2][0] - axes[0][2]) / divisor,
					(axes[0][1] - axes[1][0]) / divisor, divisor / 4};
			}
			else
			{
				std::size_t largestAxis = 0;
				for (std::size_t axis = 1; axis < 3; ++axis)
				{
					if (axes[axis][axis] > axes[largestAxis][largestAxis])
					{
						largestAxis = axis;
					}
				}
				const auto nextAxis = (largestAxis + 1) % 3;
				const auto lastAxis = (largestAxis + 2) % 3;
				const auto divisor = 2 * std::sqrt(1 + axes[largestAxis][largestAxis] - axes[nextAxis][nextAxis] - axes[lastAxis][lastAxis]);
				rotation[largestAxis] = divisor / 4;
				rotation[nextAxis] = (axes[largestAxis][nextAxis] + axes[nextAxis][largestAxis]) / divisor;
				rotation[lastAxis] = (axes[largestAxis][lastAxis] + axes[lastAxis][largestAxis]) / divisor;
				rotation[3] = (axes[nextAxis][lastAxis] - axes[lastAxis][nextAxis]) / divisor;
			}

			// Cameraのunit scale制約も使えるよう、丸め誤差範囲の+1を正確な1へそろえる。
			for (auto& scale : scales)
			{
				if (std::abs(scale - 1) <= 1.0e-6)
				{
					scale = 1;
				}
			}
			LocalTransform local;
			local.position = {matrix(3, 0), matrix(3, 1), matrix(3, 2)};
			local.scale = {CheckedFloat(scales[0]), CheckedFloat(scales[1]), CheckedFloat(scales[2])};
			local.rotation = KT::Core::Math::Normalize(
				{CheckedFloat(rotation[0]), CheckedFloat(rotation[1]), CheckedFloat(rotation[2]), CheckedFloat(rotation[3])});

			// 分解後のfloat値から再構成し、近似では保持できない行列を拒否する。
			if (!TransformNear(matrix, KT::Core::Math::LocalMatrix(local.position, local.rotation, local.scale)))
			{
				throw std::invalid_argument("KeepWorldの新Local行列をTRSで許容誤差内に再構成できません。");
			}
			return local;
		}

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

		// 番号は確保失敗時も消費する。既存Componentを全収集せず、空き順序だけを保存する。
		const auto transactionNumber = ++lastTransactionNumber_;
		auto transaction = std::make_unique<Transaction>();
		transaction->number = transactionNumber;
		transaction->initialSlotCount = slots_.size();
		transaction->freeSlots = freeSlots_;
		transaction->freeSlots.reserve(slots_.size());
		transaction->previousHierarchy = hierarchyCache_;
		transaction_ = std::move(transaction);
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

	void World::RecordAdded(EntityData& data, std::type_index type) noexcept
	{
		if (transaction_)
		{
			// 登録済みの実体に印を付ける。同型を追加し直した場合も元の実体と区別する。
			data.components.find(type)->second->createdTransaction = transaction_->number;
			auto& action = transaction_->actions.emplace_back(UndoAction::Kind::Add);
			action.data = &data;
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
		transaction_.reset();
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
				action.data->components.erase(action.type);
				break;
			case UndoAction::Kind::Remove:
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
		transaction_.reset();
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

		// 生存Entityだけを、スロット順の値として集める。
		std::vector<Entity> entities;
		entities.reserve(count_);
		for (std::size_t index = 0; index < slots_.size(); ++index)
		{
			if (slots_[index].data)
			{
				entities.push_back(At(index));
			}
		}

		return entities;
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

		// 作業索引は一度だけ全slotから作り、途中失敗した索引は公開しない。
		auto batch = std::make_unique<HierarchyBatch>();
		batch->links.resize(slots_.size());
		for (std::size_t index = 0; index < slots_.size(); ++index)
		{
			if (slots_[index].data)
			{
				batch->links[index].alive = true;
			}
		}
		for (std::size_t index = 0; index < slots_.size(); ++index)
		{
			if (!slots_[index].data)
			{
				continue;
			}
			const auto* hierarchy = FindIn<Hierarchy>(*slots_[index].data);
			if (hierarchy && hierarchy->parent != Entity{})
			{
				RequireAlive(hierarchy->parent);
				batch->SetParent(At(index).index, hierarchy->parent.index);
			}
		}

		// 根からの到達数で循環を検査する。以後の親変更は祖先列だけを検査する。
		std::vector<std::uint32_t> reached;
		reached.reserve(count_);
		for (std::size_t index = 0; index < batch->links.size(); ++index)
		{
			if (batch->links[index].alive && batch->links[index].parent == Entity::InvalidIndex)
			{
				reached.push_back(At(index).index);
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
			for (std::size_t index = 0; index < batch.links.size(); ++index)
			{
				if (batch.links[index].alive && batch.links[index].parent == Entity::InvalidIndex)
				{
					batch.parentFirst.push_back(At(index).index);
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
			for (std::size_t index = 0; index < slots_.size(); ++index)
			{
				if (slots_[index].data)
				{
					const auto* hierarchy = FindIn<Hierarchy>(*slots_[index].data);
					inputs.push_back({At(index), hierarchy ? hierarchy->parent : Entity{}});
				}
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
				RecordAdded(data, typeid(Hierarchy));
			}
			if (!transform)
			{
				RecordAdded(data, typeid(LocalTransform));
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
			ReserveUndo(1);
			data.components.emplace(typeid(Hierarchy), std::make_unique<ComponentBox<Hierarchy>>(parent));
			RecordAdded(data, typeid(Hierarchy));
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

	void World::DestroyEntity(Entity entity, std::vector<Entity>& destroyed)
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

		destroyed.reserve(destroyed.size() + subtree.size());
		ReserveUndo(subtree.size());

		// 対象を逆順に破棄し、子のcomponentを親より先に破棄する。
		MutationGuard guard(*this);
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
