#include <Physics/JoltWorldInternal.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <cmath>

namespace KT::Physics
{
	void PhysicsWorld::Apply(BodyHandle handle, const BodyCommand& command)
	{
		impl_->RequireMutable();
		Validate(command);
		auto& record = impl_->Find(handle);
		if (!record.enabled)
		{
			throw std::logic_error("無効なBodyへ物理命令を送信できません。");
		}
		const auto id = record.body->GetID();
		auto& interface = impl_->system.GetBodyInterface();
		switch (command.type)
		{
		case CommandType::Force:
		case CommandType::Impulse:
		case CommandType::Velocity:
			if (record.motion != MotionType::Dynamic)
			{
				throw std::invalid_argument("力・impulse・速度の命令はDynamic Bodyだけに使用できます。");
			}
			if (command.type == CommandType::Force)
			{
				interface.AddForceAndTorque(id, ToPolar(command.linear), ToAxial(command.angular));
			}
			else if (command.type == CommandType::Impulse)
			{
				interface.AddImpulse(id, ToPolar(command.linear));
				interface.AddAngularImpulse(id, ToAxial(command.angular));
			}
			else
			{
				interface.SetLinearAndAngularVelocity(id, ToPolar(command.linear), ToAxial(command.angular));
			}
			break;
		case CommandType::KinematicTarget:
			if (record.motion != MotionType::Kinematic)
			{
				throw std::invalid_argument("移動目標はKinematic Bodyだけに使用できます。");
			}
			interface.MoveKinematic(id, ToPolar(command.pose.position), ToRotation(command.pose.rotation), float(FixedDeltaSeconds));
			break;
		case CommandType::Teleport:
			// 移動元の支持を失うBodyと、移動先へ重なるBodyの両方を起こす。
			if (!record.creation.mIsSensor)
			{
				impl_->WakeOverlappingBodies(record.body->GetWorldSpaceBounds());
			}
			interface.SetPositionAndRotation(
				id, ToPolar(command.pose.position), ToRotation(command.pose.rotation), JPH::EActivation::Activate);
			if (!record.creation.mIsSensor)
			{
				impl_->WakeOverlappingBodies(record.body->GetWorldSpaceBounds());
			}
			if (command.clearVelocity && record.motion != MotionType::Static)
			{
				interface.SetLinearAndAngularVelocity(id, JPH::Vec3::sZero(), JPH::Vec3::sZero());
			}
			break;
		}
	}

	void PhysicsWorld::Step()
	{
		impl_->RequireMutable();
		impl_->listener.entries.clear();
		impl_->listener.failed = false;
		const auto error = impl_->system.Update(float(FixedDeltaSeconds), 1, &impl_->runtime->temporary, &impl_->runtime->jobs);
		if (error != JPH::EPhysicsUpdateError::None)
		{
			throw std::runtime_error("Joltの更新領域が不足しました。EPhysicsUpdateError=" + std::to_string(static_cast<unsigned>(error)));
		}
		if (impl_->listener.failed)
		{
			throw std::runtime_error("Joltの接触イベントを保存するメモリを確保できませんでした。");
		}
		// Kinematicの目標は1tick限り。次tickの命令がなければ停止する。
		for (auto& [handle, record] : impl_->bodies)
		{
			const auto state = GetState(handle);
			Validate(state.pose);
			if (!KT::Core::Math::IsFinite(state.linearVelocity) || !KT::Core::Math::IsFinite(state.angularVelocity))
			{
				throw std::runtime_error("Joltの計算結果に有限でない速度が含まれています。");
			}
			if (record.motion == MotionType::Kinematic)
			{
				impl_->system.GetBodyInterface().SetLinearAndAngularVelocity(record.body->GetID(), JPH::Vec3::sZero(), JPH::Vec3::sZero());
			}
		}
	}

	std::vector<ContactEvent> PhysicsWorld::Contacts() const
	{
		std::vector<ContactEvent> events;
		events.reserve(impl_->listener.entries.size());
		for (const auto& contact : impl_->listener.entries)
		{
			const auto first = impl_->handles.find(contact.first.GetIndexAndSequenceNumber());
			const auto second = impl_->handles.find(contact.second.GetIndexAndSequenceNumber());
			if (first == impl_->handles.end() || second == impl_->handles.end() ||
				impl_->retired.contains(contact.first.GetIndexAndSequenceNumber()) ||
				impl_->retired.contains(contact.second.GetIndexAndSequenceNumber()))
			{
				continue;
			}
			const auto& firstBody = impl_->Find(first->second);
			const auto& secondBody = impl_->Find(second->second);
			events.push_back({contact.type, first->second, second->second, contact.normal,
				firstBody.creation.mIsSensor || secondBody.creation.mIsSensor});
		}
		return events;
	}

	std::optional<RayHit> PhysicsWorld::CastRay(const Ray& ray) const
	{
		if (!KT::Core::Math::IsFinite(ray.origin) || !std::isfinite(ray.maxDistance) || ray.maxDistance <= 0)
		{
			throw std::invalid_argument("物理rayの始点・長さが不正です。");
		}
		const auto direction = KT::Core::Math::Normalized(ray.direction) * ray.maxDistance;
		if (!KT::Core::Math::IsFinite(direction))
		{
			throw std::invalid_argument("物理rayの終点方向がfloatの範囲外です。");
		}

		// Query専用filterでsensor・ゲーム層・自己Bodyを除外する。
		class QueryFilter final : public JPH::BodyFilter
		{
		public:
			const Ray& ray;
			JPH::BodyID ignore;

			QueryFilter(const Ray& input, JPH::BodyID ignored)
				: ray(input),
				  ignore(ignored)
			{
			}

			bool ShouldCollide(const JPH::BodyID& id) const override
			{
				return id != ignore;
			}

			bool ShouldCollideLocked(const JPH::Body& body) const override
			{
				return (ray.includeSensors || !body.IsSensor()) && (ray.layerMask & (1u << (body.GetObjectLayer() % LayerCount))) != 0;
			}
		};

		const JPH::BodyID ignored = ray.ignore ? impl_->Find(*ray.ignore).body->GetID() : JPH::BodyID{};
		QueryFilter filter(ray, ignored);
		JPH::RRayCast cast(ToPolar(ray.origin), ToPolar(direction));
		JPH::RayCastResult hit;
		if (!impl_->system.GetNarrowPhaseQuery().CastRay(cast, hit, {}, {}, filter))
		{
			return std::nullopt;
		}
		const auto handle = impl_->handles.at(hit.mBodyID.GetIndexAndSequenceNumber());
		const auto& body = *impl_->Find(handle).body;
		const auto position = cast.GetPointOnRay(hit.mFraction);
		// 内部から始まるrayは表面を持たないため、入射方向の反対を法線として返す。
		const auto normal = hit.mFraction == 0 ? -KT::Core::Math::Normalized(ray.direction) :
												 FromPolar(body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, position));
		return RayHit{handle, FromPolar(position), normal, ray.maxDistance * hit.mFraction};
	}
}
