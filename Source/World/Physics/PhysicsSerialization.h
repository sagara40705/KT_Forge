#pragma once
#include <World/Physics/PhysicsComponents.h>
#include <World/Scene/SceneValue.h>

namespace KT::World
{
	class ComponentRegistry;
	struct SceneAsset;
	// 実行状態を除いた設定だけを保存する。Joltの初期化は不要。
	SceneValue EncodePhysics(const KT::Physics::BodySettings& settings);
	SceneValue EncodePhysics(const KT::Physics::ColliderSettings& settings);
	SceneValue EncodePhysics(const KT::Physics::SceneSettings& settings);
	KT::Physics::BodySettings DecodeBodySettings(const SceneValue& data);
	KT::Physics::ColliderSettings DecodeColliderSettings(const SceneValue& data);
	KT::Physics::SceneSettings DecodePhysicsSettings(const SceneValue& data);
	void RegisterPhysicsComponents(ComponentRegistry& registry);
	void ValidatePhysicsObjects(const SceneAsset& asset);
}
