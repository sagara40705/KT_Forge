#include <World/World.h>
#include <World/Scene/Hierarchy.h>
#include <World/Systems/HierarchySystem.h>
#include <algorithm>
#include <atomic>
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
		if (freeSlots_.empty())
		{
			// 各slotを破棄時に確保なしで戻せる容量を、作成時に用意する。
			const auto required = slots_.size() + 1;
			if (freeSlots_.capacity() < required)
			{
				const auto growth = freeSlots_.capacity() + freeSlots_.capacity() / 2 + 1;
				freeSlots_.reserve((std::max)(required, growth));
			}
			slots_.emplace_back();
		}
		else
		{
			freeSlots_.pop_back();
		}

		slots_[index].data = std::move(data);
		++count_;
		return At(index);
	}

	void World::SetParent(Entity child, Entity parent)
	{
		RequireStructuralChange();
		RequireAlive(child);
		if (parent != Entity{})
		{
			RequireAlive(parent);
		}

		// 親の置換後の全階層を検証し、循環を作らない。
		SceneUpdateContext context(*this, std::nullopt, {});
		(void)HierarchySystem::Build(context.Inputs(), std::pair{child, parent});
		MutationGuard guard(*this);
		auto& data = *slots_[child.index].data;
		if (auto* hierarchy = FindIn<Hierarchy>(data))
		{
			hierarchy->parent = parent;
		}
		else
		{
			data.components.emplace(typeid(Hierarchy), std::make_unique<ComponentBox<Hierarchy>>(parent));
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
		// componentを破棄してから世代を進め、古いEntityを失効させる。
		auto& slot = slots_[entity.index];
		slot.data.reset();

		// 上限世代のslotは0で退役。古いhandleと一致する世代へ循環させない。
		slot.generation = NextGeneration(slot.generation);
		--count_;
		if (slot.generation != 0)
		{
			freeSlots_.push_back(entity.index);
		}
	}
}
