#include <World/World.h>
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
				throw std::overflow_error("Entity indexが枯渇しました。");
			return static_cast<std::uint32_t>(index);
		}

		std::uint64_t AcquireWorldId()
		{
			auto candidate = nextWorldId.load(std::memory_order_relaxed);
			for (;;)
			{
				if (candidate == (std::numeric_limits<std::uint64_t>::max)())
					throw std::overflow_error("World IDが枯渇しました。");
				if (nextWorldId.compare_exchange_weak(candidate, candidate + 1, std::memory_order_relaxed))
					return candidate;
			}
		}
	}

	World::World() : worldId_(AcquireWorldId()) {}

	World::~World()
	{
		// 実行中のWorldを破棄してcallback/guardの参照を失効させない。
		if (mutating_ || enumerations_ != 0) std::terminate();
		// component破棄中も内部状態が有効な間に再入を拒否する。
		mutating_ = true;
		slots_.clear();
	}

	void World::RequireReadable() const
	{
		if (mutating_)
			throw std::logic_error("componentの構築/破棄または構造変更中はWorldへ再入できません。");
	}

	World::MutationGuard::MutationGuard(World& world) : world_(world)
	{
		world_.RequireStructuralChange();
		world_.mutating_ = true;
	}
	World::MutationGuard::~MutationGuard()
	{
		world_.mutating_ = false;
	}
	World::EnumerationGuard::EnumerationGuard(const World& world) : world_(world)
	{
		world_.RequireReadable();
		if (world_.enumerations_ == (std::numeric_limits<std::size_t>::max)())
			throw std::overflow_error("列挙の入れ子数が上限に達しました。");
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
		return entity.IsValid() && entity.worldId == worldId_ && entity.index < slots_.size() &&
			slots_[entity.index].data && slots_[entity.index].generation == entity.generation;
	}
	bool World::IsAlive(Entity entity) const
	{
		RequireReadable();
		return IsAliveUnchecked(entity);
	}
	std::vector<Entity> World::Entities() const
	{
		RequireReadable();
		std::vector<Entity> result;
		result.reserve(count_);
		for (std::size_t index = 0; index < slots_.size(); ++index)
			if (slots_[index].data) result.push_back(At(index));
		return result;
	}
	void World::RequireStructuralChange() const
	{
		RequireReadable();
		if (enumerations_ != 0)
			throw std::logic_error("列挙中はWorldの構造を変更できません。");
	}
	void World::RequireAlive(Entity entity) const
	{
		if (!IsAliveUnchecked(entity))
			throw std::invalid_argument("Entityが無効/失効済み、または別Worldのものです。");
	}
	Entity World::At(std::size_t index) const noexcept
	{
		return {worldId_, static_cast<std::uint32_t>(index), slots_[index].generation};
	}

	Entity World::CreateEntity()
	{
		MutationGuard guard(*this);
		std::size_t index = 0;
		for (; index < slots_.size(); ++index)
			if (!slots_[index].data && slots_[index].generation != 0) break;
		(void)CheckedIndex(index);
		// 確保は公開前に済ませる。vector拡張失敗でも生存数/既存slotは変わらない。
		auto data = std::make_unique<EntityData>();
		if (index == slots_.size()) slots_.emplace_back();
		slots_[index].data = std::move(data);
		++count_;
		return At(index);
	}

	void World::DestroyEntity(Entity entity)
	{
		MutationGuard guard(*this);
		RequireAlive(entity);
		auto& slot = slots_[entity.index];
		slot.data.reset();
		// 上限世代のslotは0で退役。古いhandleと一致する世代へ循環させない。
		slot.generation = NextGeneration(slot.generation);
		--count_;
	}
}
