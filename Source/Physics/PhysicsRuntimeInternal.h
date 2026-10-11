#pragma once
// Joltの型を公開headerから隔離する。Physics内部のcppだけが使用する。
#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Physics/PhysicsRuntime.h>

namespace KT::Physics
{
	struct PhysicsRuntime::State
	{
		State();
		~State();
		// 同時更新は行わない。各WorldのUpdateは呼出threadで完了する。
		JPH::JobSystemSingleThreaded jobs{2048};
		JPH::TempAllocatorImplWithMallocFallback temporary{32 * 1024 * 1024};
	};
}
