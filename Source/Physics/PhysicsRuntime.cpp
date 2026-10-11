#include <Physics/PhysicsRuntimeInternal.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <stdexcept>

namespace KT::Physics
{
	PhysicsRuntime::State::State()
	{
		// allocatorは実行資源の構築前にPhysicsRuntime側で登録済み。
		if (JPH::Factory::sInstance)
		{
			throw std::logic_error("Jolt Factoryが既に別の所有者によって登録されています。");
		}
		auto factory = std::make_unique<JPH::Factory>();
		JPH::Factory::sInstance = factory.get();
		try
		{
			JPH::RegisterTypes();
		}
		catch (...)
		{
			JPH::Factory::sInstance = nullptr;
			throw;
		}
		(void)factory.release();
	}

	PhysicsRuntime::State::~State()
	{
		JPH::UnregisterTypes();
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}

	PhysicsRuntime::PhysicsRuntime()
	{
		// 起動・終了も製品の単一thread契約で行う。weak参照は寿命を延長しない。
		static std::weak_ptr<State> sharedState;
		state_ = sharedState.lock();
		if (!state_)
		{
			JPH::RegisterDefaultAllocator();
			state_ = std::make_shared<State>();
			sharedState = state_;
		}
	}

	PhysicsRuntime::~PhysicsRuntime() = default;
}
