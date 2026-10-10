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

		// 空き順序と既存実体の識別だけを捕捉し、Worldの所有構造はコピーしない。
		auto transaction = std::make_unique<Transaction>();
		transaction->initialSlotCount = slots_.size();
		transaction->freeSlots = freeSlots_;
		transaction->freeSlots.reserve(slots_.size());
		for (const auto& slot : slots_)
		{
			if (slot.data)
			{
				for (const auto& [type, component] : slot.data->components)
				{
					(void)type;
					transaction->originalComponents.push_back(component.get());
				}
			}
		}
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

	void World::RecordAdded(EntityData& data, std::type_index type) noexcept
	{
		if (transaction_)
		{
			auto& action = transaction_->actions.emplace_back(UndoAction::Kind::Add);
			action.data = &data;
			action.type = type;
		}
	}

	bool World::IsOriginalComponent(ComponentBase* component) const noexcept
	{
		return transaction_ && std::find(transaction_->originalComponents.begin(), transaction_->originalComponents.end(), component) !=
			transaction_->originalComponents.end();
	}

	void World::BackupScript(ScriptBehaviour& script)
	{
		MutationGuard guard(*this);
		transaction_->values.reserve(transaction_->values.size() + 1);
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
				slots_[action.index].data.reset();
				slots_[action.index].generation = NextGeneration(slots_[action.index].lastIssuedGeneration);
				--count_;
				break;
			case UndoAction::Kind::Destroy:
				slots_[action.index].data = std::move(action.destroyed);
				slots_[action.index].generation = action.generation;
				++count_;
				break;
			case UndoAction::Kind::Add:
				action.data->components.erase(action.type);
				break;
			case UndoAction::Kind::Remove:
				action.data->components.insert(std::move(action.removed));
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
		for (const auto& slot : slots_)
		{
			if (slot.data)
			{
				const auto* identity = FindIn<PersistentId>(*slot.data);
				if (identity && identity->value == uuid)
				{
					throw std::invalid_argument("生成するSceneオブジェクトのUUIDが同じWorld内で重複しています。");
				}
			}
		}

		// 必須componentの構築と登録を済ませてから、Entityを公開する。
		auto data = std::make_unique<EntityData>();
		data->components.emplace(typeid(PersistentId), std::make_unique<ComponentBox<PersistentId>>(uuid));
		data->components.emplace(typeid(Name), std::make_unique<ComponentBox<Name>>(std::move(name)));
		return PublishEntity(std::move(data));
	}

	Entity World::PublishEntity(std::unique_ptr<EntityData> data)
	{
		// 空きslotの取得は定数時間。確保失敗時はfree listを消費しない。
		const auto index = freeSlots_.empty() ? CheckedIndex(slots_.size()) : freeSlots_.back();
		ReserveUndo(1);
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
				transaction_->freeSlots.reserve(required);
			}
			slots_.emplace_back();
		}
		else
		{
			freeSlots_.pop_back();
		}

		if (transaction_)
		{
			auto& action = transaction_->actions.emplace_back(UndoAction::Kind::Create);
			action.index = index;
		}
		slots_[index].data = std::move(data);
		slots_[index].lastIssuedGeneration = slots_[index].generation;
		++count_;
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

		// 親の置換後の全階層を検証し、循環を作らない。
		SceneUpdateContext context(*this, std::nullopt, {});
		(void)HierarchySystem::Build(context.Inputs(), std::pair{child, parent});

		if (mode == ParentChangeMode::KeepWorld)
		{
			auto& data = *slots_[child.index].data;
			auto* hierarchy = FindIn<Hierarchy>(data);
			if ((hierarchy ? hierarchy->parent : Entity{}) == parent)
			{
				return;
			}

			// 現在のWorldを再計算する。予約より前のsnapshotを使わず、先行操作を含める。
			HierarchySystem{}.Update(context);
			ActivationSystem{}.Update(context);
			TransformSystem{}.Update(context);
			KT::Core::Math::Matrix4 oldWorld;
			KT::Core::Math::Matrix4 parentWorld;
			for (const auto& finalized : context.GetTransforms())
			{
				if (finalized.entity == child)
				{
					oldWorld = finalized.transform.matrix;
				}
				if (finalized.entity == parent)
				{
					parentWorld = finalized.transform.matrix;
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
			return;
		}

		// KeepLocalは既存のローカル値とcomponent欠落を保つ。
		MutationGuard guard(*this);
		auto& data = *slots_[child.index].data;
		if (auto* hierarchy = FindIn<Hierarchy>(data))
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
		const auto hierarchy = ValidateHierarchy(*this);
		std::vector<std::size_t> subtree;
		for (std::size_t index = 0; index < hierarchy.nodes.size(); ++index)
		{
			if (hierarchy.nodes[index].entity == entity)
			{
				subtree.push_back(index);
				break;
			}
		}
		for (std::size_t index = 0; index < subtree.size(); ++index)
		{
			for (const auto child : hierarchy.nodes[subtree[index]].children)
			{
				subtree.push_back(child);
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
			const auto target = hierarchy.nodes[*iterator].entity;
			destroyed.push_back(target);
			DestroySlot(target);
		}
	}

	void World::DestroySlot(Entity entity) noexcept
	{
		// Entityを失効させる。更新中の実体解放は成功確定まで延期する。
		auto& slot = slots_[entity.index];
		if (transaction_)
		{
			auto& action = transaction_->actions.emplace_back(UndoAction::Kind::Destroy);
			action.index = entity.index;
			action.generation = slot.generation;
			action.destroyed = std::move(slot.data);
		}
		else
		{
			slot.data.reset();
		}

		// 上限世代のslotは0で退役。古いhandleと一致する世代へ循環させない。
		slot.generation = NextGeneration(slot.lastIssuedGeneration);
		--count_;
		if (slot.generation != 0)
		{
			freeSlots_.push_back(entity.index);
		}
	}
}
