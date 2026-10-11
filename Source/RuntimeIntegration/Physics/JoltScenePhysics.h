#pragma once
#include <World/Scene/Scene.h>
#include <Physics/PhysicsWorld.h>

namespace KT::RuntimeIntegration
{
	// Worldと同一寿命のBody対応表を持つ。製品側はこの関数で候補を完成してからQueueSceneする。
	// 失敗時はSceneの既存接続を変更しない。JSON読込・素材ツールでは呼ばない。
	void PrepareScenePhysics(KT::World::Scene& scene, KT::Physics::PhysicsRuntime& runtime);
}
