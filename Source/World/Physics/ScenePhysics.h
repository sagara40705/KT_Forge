#pragma once
#include <World/Entity.h>
#include <Physics/PhysicsTypes.h>
#include <Core/Utility/NonCopyable.h>
#include <span>

namespace KT::World
{
	class World;
	class Scene;
	class SceneUpdateContext;

	struct PhysicsContact
	{
		KT::Physics::ContactType type;
		Entity first;
		Entity second;
		KT::Core::Math::Vector3 normal;
		bool sensor = false;
	};

	struct PhysicsRayHit
	{
		Entity entity;
		KT::Core::Math::Vector3 position;
		KT::Core::Math::Vector3 normal;
		float distance = 0;
	};

	// 内部書戻し用の所有値。ゲーム側へ可変component参照を渡さない。
	struct PhysicsWriteback
	{
		Entity entity;
		KT::Physics::BodyState state;
	};

	// Scene専用の物理接続口。実装の所有はScene、Joltとの対応付けはRuntimeIntegrationが担当する。
	class ScenePhysics : private KT::Core::NonCopyable
	{
	public:
		virtual ~ScenePhysics() = default;
		// ゲームcallback内で予約し、callback完了後に順番どおり反映する。
		virtual void Enqueue(Entity entity, const KT::Physics::BodyCommand& command) = 0;
		// callback中は開始境界の状態、それ以外は最新の成功状態を問い合わせる。
		virtual std::optional<PhysicsRayHit> CastRay(const KT::Physics::Ray& ray, std::optional<Entity> ignore = {}) const = 0;
		// 前回成功tickのイベント。次の成功commitまで有効。削除対象は配信前に除外する。
		virtual std::span<const PhysicsContact> Contacts() const noexcept = 0;

	private:
		friend class Scene;
		virtual void Begin(const World& world, const SceneUpdateContext& cpu) = 0;
		virtual std::span<const PhysicsWriteback> Simulate(const World& world) = 0;
		virtual void Finish(const World& world, const SceneUpdateContext& cpu) = 0;
		virtual void Commit() noexcept = 0;
		virtual void Rollback() noexcept = 0;
	};
}
