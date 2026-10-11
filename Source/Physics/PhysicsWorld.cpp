#include <Physics/JoltWorldInternal.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <limits>

namespace KT::Physics
{
	PhysicsWorld::Impl::Impl(std::shared_ptr<PhysicsRuntime::State> owner, const SceneSettings& input)
		: runtime(std::move(owner)),
		  settings(input),
		  pairFilter(input)
	{
		Validate(settings);
		system.Init(MaxBodies, 0, 16384, 16384, layers, broadFilter, pairFilter);
		system.SetGravity(ToPolar(settings.gravity));
		system.SetContactListener(&listener);
	}

	PhysicsWorld::Impl::~Impl()
	{
		// callbackの寿命を切ってからBodyを全て解放する。
		system.SetContactListener(nullptr);
		auto& interface = system.GetBodyInterface();
		for (auto& [handle, record] : bodies)
		{
			if (!record.body)
			{
				continue;
			}
			if (record.enabled)
			{
				interface.RemoveBody(record.body->GetID());
			}
			interface.DestroyBody(record.body->GetID());
		}
	}

	PhysicsWorld::Impl::Record& PhysicsWorld::Impl::Find(BodyHandle handle)
	{
		const auto found = bodies.find(handle);
		if (found == bodies.end())
		{
			throw std::invalid_argument("物理Bodyが存在しないか、既に削除されています。");
		}
		return found->second;
	}

	const PhysicsWorld::Impl::Record& PhysicsWorld::Impl::Find(BodyHandle handle) const
	{
		return const_cast<Impl*>(this)->Find(handle);
	}

	void PhysicsWorld::Impl::RequireMutable() const
	{
		if (sealed)
		{
			throw std::logic_error("確定済みの物理worldは変更できません。Cloneで作業候補を作成してください。");
		}
	}

	void PhysicsWorld::Impl::WakeOverlappingBodies(JPH::AABox bounds)
	{
		// 接触許容距離だけ離れて静止している物体も対象へ含める。
		bounds.ExpandBy(JPH::Vec3::sReplicate(system.GetPhysicsSettings().mSpeculativeContactDistance));
		system.GetBodyInterface().ActivateBodiesInAABox(bounds, {}, {});
	}

	PhysicsWorld::PhysicsWorld(PhysicsRuntime& runtime, const SceneSettings& settings)
		: impl_(std::make_unique<Impl>(runtime.state_, settings))
	{
	}

	PhysicsWorld::PhysicsWorld(std::unique_ptr<Impl> implementation)
		: impl_(std::move(implementation))
	{
	}

	PhysicsWorld::~PhysicsWorld() = default;

	BodyHandle PhysicsWorld::Add(const BodySettings& body, const ColliderSettings& collider, const Pose& pose, bool enabled)
	{
		impl_->RequireMutable();
		Validate(body);
		Validate(collider);
		Validate(pose);
		if (impl_->nextHandle == (std::numeric_limits<std::uint64_t>::max)())
		{
			throw std::overflow_error("物理Bodyの論理IDが上限に達しました。");
		}

		// 指定IDでの復元はJolt内部の採番世代を更新しないため、通常生成もこちらで採番する。
		JPH::BodyID id;
		for (JPH::uint32 index = 0; index < Impl::MaxBodies && id.IsInvalid(); ++index)
		{
			const auto& slot = impl_->bodySlots[index];
			if (slot.occupied)
			{
				continue;
			}
			for (unsigned advance = 1; advance <= 256; ++advance)
			{
				const auto sequence = static_cast<JPH::uint8>((unsigned(slot.sequence) + advance) & 255u);
				const JPH::BodyID candidate(index, sequence);
				// 8bit世代が一周しても、この候補の古いcontact・通知に残るIDは使わない。
				if (!impl_->retired.contains(candidate.GetIndexAndSequenceNumber()))
				{
					id = candidate;
					break;
				}
			}
		}
		if (id.IsInvalid())
		{
			throw std::runtime_error("物理Bodyの空きIDがありません。上限4096体、または同一候補内の世代消費を確認してください。");
		}

		// shapeは検査付きSettingsで生成し、成功後だけBodyへ渡す。
		JPH::ShapeSettings::ShapeResult shape;
		switch (collider.shape)
		{
		case ShapeType::Box:
			shape = JPH::BoxShapeSettings(JPH::Vec3(collider.halfExtent.x, collider.halfExtent.y, collider.halfExtent.z), 0.0f).Create();
			break;
		case ShapeType::Sphere:
			shape = JPH::SphereShapeSettings(collider.radius).Create();
			break;
		case ShapeType::Capsule:
			shape = JPH::CapsuleShapeSettings(collider.halfHeight, collider.radius).Create();
			break;
		}
		if (shape.HasError())
		{
			throw std::invalid_argument("Collider形状を生成できません: " + std::string(shape.GetError().c_str()));
		}
		shape = JPH::RotatedTranslatedShapeSettings(ToPolar(collider.offset.position), ToRotation(collider.offset.rotation), shape.Get())
					.Create();
		if (shape.HasError())
		{
			throw std::invalid_argument("Colliderの局所変換を生成できません: " + std::string(shape.GetError().c_str()));
		}

		const auto motion = body.motion == MotionType::Static ? JPH::EMotionType::Static :
			body.motion == MotionType::Dynamic				  ? JPH::EMotionType::Dynamic :
																JPH::EMotionType::Kinematic;
		const auto layer = static_cast<JPH::ObjectLayer>(collider.layer + (body.motion == MotionType::Static ? 0 : LayerCount));
		JPH::BodyCreationSettings creation(shape.Get(), ToPolar(pose.position), ToRotation(pose.rotation), motion, layer);
		creation.mFriction = body.friction;
		creation.mRestitution = body.restitution;
		creation.mLinearDamping = body.linearDamping;
		creation.mAngularDamping = body.angularDamping;
		creation.mGravityFactor = body.gravityFactor;
		creation.mMaxLinearVelocity = MaxLinearVelocity;
		creation.mMaxAngularVelocity = MaxAngularVelocity;
		creation.mLinearVelocity = ToPolar(body.linearVelocity);
		creation.mAngularVelocity = ToAxial(body.angularVelocity);
		creation.mAllowSleeping = body.allowSleeping;
		creation.mIsSensor = collider.sensor;
		creation.mCollideKinematicVsNonDynamic = collider.sensor;
		creation.mMotionQuality = body.continuous ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
		creation.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
		creation.mMassPropertiesOverride.mMass = body.mass;

		// mapの確保を先に完了し、Jolt Bodyを取り残さない。
		const BodyHandle handle{impl_->nextHandle++};
		auto [entry, inserted] = impl_->bodies.emplace(handle, Impl::Record{nullptr, creation, body.motion, enabled});
		auto& interface = impl_->system.GetBodyInterface();
		auto* instance = interface.CreateBodyWithID(id, creation);
		if (!instance)
		{
			impl_->bodies.erase(entry);
			throw std::runtime_error("Jolt Bodyを生成できません。Sceneの上限4096体を確認してください。");
		}
		try
		{
			impl_->handles.emplace(instance->GetID().GetIndexAndSequenceNumber(), handle);
		}
		catch (...)
		{
			interface.DestroyBody(instance->GetID());
			impl_->bodies.erase(entry);
			throw;
		}
		entry->second.body = instance;
		impl_->bodySlots[id.GetIndex()] = {id.GetSequenceNumber(), true};
		if (enabled)
		{
			interface.AddBody(instance->GetID(), JPH::EActivation::Activate);
			if (!collider.sensor)
			{
				impl_->WakeOverlappingBodies(instance->GetWorldSpaceBounds());
			}
		}
		return handle;
	}

	void PhysicsWorld::Remove(BodyHandle handle)
	{
		impl_->RequireMutable();
		const auto& record = impl_->Find(handle);
		const auto id = record.body->GetID();
		impl_->retired.insert(id.GetIndexAndSequenceNumber());
		auto& interface = impl_->system.GetBodyInterface();
		if (record.enabled)
		{
			if (!record.creation.mIsSensor)
			{
				impl_->WakeOverlappingBodies(record.body->GetWorldSpaceBounds());
			}
			interface.RemoveBody(id);
		}
		interface.DestroyBody(id);
		impl_->bodySlots[id.GetIndex()].occupied = false;
		impl_->handles.erase(id.GetIndexAndSequenceNumber());
		impl_->bodies.erase(handle);
	}

	void PhysicsWorld::SetEnabled(BodyHandle handle, bool enabled)
	{
		impl_->RequireMutable();
		auto& record = impl_->Find(handle);
		if (record.enabled == enabled)
		{
			return;
		}
		const auto id = record.body->GetID();
		auto& interface = impl_->system.GetBodyInterface();
		if (enabled)
		{
			interface.AddBody(id, JPH::EActivation::Activate);
			record.enabled = true;
			if (!record.creation.mIsSensor)
			{
				impl_->WakeOverlappingBodies(record.body->GetWorldSpaceBounds());
			}
		}
		else
		{
			impl_->retired.insert(id.GetIndexAndSequenceNumber());
			if (!record.creation.mIsSensor)
			{
				impl_->WakeOverlappingBodies(record.body->GetWorldSpaceBounds());
			}
			// JoltのRemoveBodyは速度をゼロにするため、無効化前の速度を退避して戻す。
			JPH::Vec3 linearVelocity;
			JPH::Vec3 angularVelocity;
			interface.GetLinearAndAngularVelocity(id, linearVelocity, angularVelocity);
			interface.RemoveBody(id);
			interface.SetLinearAndAngularVelocity(id, linearVelocity, angularVelocity);
		}
		record.enabled = enabled;
	}

	BodyState PhysicsWorld::GetState(BodyHandle handle) const
	{
		const auto& record = impl_->Find(handle);
		const auto& body = *record.body;
		BodyState state;
		state.pose = {FromPolar(body.GetPosition()), FromRotation(body.GetRotation())};
		state.enabled = record.enabled;
		state.sleeping = !body.IsActive();
		if (!body.IsStatic())
		{
			state.linearVelocity = FromPolar(body.GetLinearVelocity());
			state.angularVelocity = FromAxial(body.GetAngularVelocity());
		}
		return state;
	}

	std::size_t PhysicsWorld::BodyCount() const noexcept
	{
		return impl_->bodies.size();
	}
}
