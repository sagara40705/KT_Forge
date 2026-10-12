#include <PhysicsDemo.h>
#include <World/Physics/PhysicsSerialization.h>
#include <World/Scene/SceneLoader.h>
#include <BuiltinAssets/StaticMeshAssets.h>

namespace KT::Sandbox
{
	std::unique_ptr<KT::World::Scene> CreatePhysicsDemoScene()
	{
		using namespace KT::World;
		using namespace KT::Physics;
		SceneAsset asset;
		asset.version = 2;
		asset.name = "JoltPhysicsDemo";
		asset.physics = SceneSettings{};

		// 衝突rootは単位scale、描画用Cubeは子として寸法を合わせる。
		for (int index = 0; index < 5; ++index)
		{
			const bool floor = index == 0;
			BodySettings body;
			body.motion = floor ? MotionType::Static : MotionType::Dynamic;
			ColliderSettings collider;
			collider.halfExtent = floor ? Vector3{5, 0.25f, 4} : Vector3{0.5f, 0.5f, 0.5f};
			SceneObjectDefinition root;
			root.uuid = ObjectUuid::Generate();
			root.name = floor ? "Floor" : "FallingBox" + std::to_string(index);
			root.components = {{"kt.RigidBody", 1, EncodePhysics(body)}, {"kt.Collider", 1, EncodePhysics(collider)},
				{"kt.LocalTransform", 1,
					SceneValue::Object{
						{"position", SceneValue::Array{floor ? 0.0 : (index - 2.5) * 1.2, floor ? -0.5 : 1.5 + index * 0.9, 0.0}},
						{"rotation", SceneValue::Array{0.0, 0.0, 0.0, 1.0}}, {"scale", SceneValue::Array{1.0, 1.0, 1.0}}}}};

			SceneObjectDefinition visual;
			visual.uuid = ObjectUuid::Generate();
			visual.parent = root.uuid;
			visual.name = root.name + "Visual";
			visual.components = {{"kt.MeshRenderer", 3,
									 SceneValue::Object{{"mesh", KT::BuiltinAssets::CubeAssetId().ToString()},
										 {"material", KT::BuiltinAssets::WhiteMaterialAssetId().ToString()}, {"visible", true},
										 {"materials", SceneValue::Object{}}}},
				{"kt.LocalTransform", 1,
					SceneValue::Object{{"position", SceneValue::Array{0.0, 0.0, 0.0}}, {"rotation", SceneValue::Array{0.0, 0.0, 0.0, 1.0}},
						{"scale",
							SceneValue::Array{double(collider.halfExtent.x * 2), double(collider.halfExtent.y * 2),
								double(collider.halfExtent.z * 2)}}}}};
			asset.objects.push_back(std::move(root));
			asset.objects.push_back(std::move(visual));
		}

		SceneObjectDefinition camera;
		camera.uuid = ObjectUuid::Generate();
		camera.name = "Camera";
		const auto rotation = KT::Core::Math::FromAxisAngle({1, 0, 0}, 0.20f);
		camera.components = {
			{"kt.Camera", 1, SceneValue::Object{{"verticalFovRadians", 1.0471975512}, {"nearPlane", 0.1}, {"farPlane", 100.0}}},
			{"kt.LocalTransform", 1,
				SceneValue::Object{{"position", SceneValue::Array{0.0, 3.5, -11.0}},
					{"rotation", SceneValue::Array{double(rotation.x), double(rotation.y), double(rotation.z), double(rotation.w)}},
					{"scale", SceneValue::Array{1.0, 1.0, 1.0}}}}};
		asset.objects.push_back(std::move(camera));
		return SceneLoader::Load(asset);
	}
}
