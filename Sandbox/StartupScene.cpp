#include <StartupScene.h>
#include <BuiltinAssets/StaticMeshAssets.h>
#include <World/Scene/SceneLoader.h>

namespace KT::Sandbox
{
	std::unique_ptr<KT::World::Scene> CreateStartupScene()
	{
		using KT::World::SceneValue;
		KT::World::SceneAsset asset;
		asset.name = "DefaultScene";
		KT::World::SceneObjectDefinition cube;
		cube.uuid = KT::World::ObjectUuid::Generate();
		cube.name = "Cube";
		cube.components.push_back({"kt.MeshRenderer", 2,
			SceneValue::Object{{"mesh", KT::BuiltinAssets::CubeAssetId().ToString()},
				{"material", KT::BuiltinAssets::WhiteMaterialAssetId().ToString()}, {"visible", true}}});
		asset.objects.push_back(std::move(cube));
		KT::World::SceneObjectDefinition camera;
		camera.uuid = KT::World::ObjectUuid::Generate();
		camera.name = "Camera";
		camera.components.push_back({"kt.LocalTransform", 1,
			SceneValue::Object{{"position", SceneValue::Array{0.0, 0.0, -3.0}}, {"rotation", SceneValue::Array{0.0, 0.0, 0.0, 1.0}},
				{"scale", SceneValue::Array{1.0, 1.0, 1.0}}}});
		camera.components.push_back(
			{"kt.Camera", 1, SceneValue::Object{{"verticalFovRadians", 1.0471975512}, {"nearPlane", 0.1}, {"farPlane", 1000.0}}});
		asset.objects.push_back(std::move(camera));
		return KT::World::SceneLoader::Load(asset);
	}
}
