#pragma once
#include <Physics/PhysicsRuntime.h>
#include <Physics/PhysicsTypes.h>
#include <span>
#include <vector>

namespace KT::Physics
{
	// 一つの剛体worldを所有する。Entity・Scene・描画資源を参照しない。
	// 全APIは単一thread専用。確定済みworldはCloneの読取以外で変更しない。
	class PhysicsWorld : private KT::Core::NonCopyable
	{
	public:
		PhysicsWorld(PhysicsRuntime& runtime, const SceneSettings& settings);
		~PhysicsWorld();
		BodyHandle Add(const BodySettings& body, const ColliderSettings& collider, const Pose& pose, bool enabled);
		void Remove(BodyHandle body);
		void SetEnabled(BodyHandle body, bool enabled);
		[[nodiscard]] BodyState GetState(BodyHandle body) const;
		void Apply(BodyHandle body, const BodyCommand& command);
		void Step();
		[[nodiscard]] std::optional<RayHit> CastRay(const Ray& ray) const;
		[[nodiscard]] std::vector<ContactEvent> Contacts() const;
		// checkpointを候補側で完成させてから所有者がcommitする。
		void Seal();
		// 同じBodyID・形状・設定を構築してから、完成checkpointを復元する。
		// 確定worldは変更しない。失敗した候補は呼出側が廃棄できる。
		[[nodiscard]] std::unique_ptr<PhysicsWorld> Clone() const;
		[[nodiscard]] std::size_t BodyCount() const noexcept;

	private:
		struct Impl;
		explicit PhysicsWorld(std::unique_ptr<Impl> implementation);
		std::unique_ptr<Impl> impl_;
	};
}
