#include <Application/AnimationDemo.h>
#include <BuiltinAssets/StaticMeshAssets.h>
#include <World/Scene/SceneLoader.h>

namespace KT::Application
{
	namespace
	{
		KT::Content::MeshAssetId AnimatedMeshId()
		{
			return KT::Content::MeshAssetId::Parse("4bc738c1-ce96-46ed-93bf-12caa49a1001");
		}
	}

	void RegisterAnimationDemoAssets(KT::Content::AssetCatalog& catalog)
	{
		using namespace KT::Content;
		using namespace KT::Core::Math;
		const auto cube = KT::BuiltinAssets::CreateCubeAsset();
		const auto& original = std::get<MeshAsset>(cube.data);
		auto skin = std::make_shared<SkinDefinition>();
		skin->joints = {{"Root"}, {"Upper", 0}};
		skin->bindings = {{0, {}}, {1, {}}};
		AnimationClip sway;
		sway.name = "Sway";
		sway.duration = 2;
		sway.tracks.resize(2);
		sway.tracks[1].rotations = {
			{0, FromAxisAngle({0, 0, 1}, -0.6f)}, {1, FromAxisAngle({0, 0, 1}, 0.6f)}, {2, FromAxisAngle({0, 0, 1}, -0.6f)}};
		AnimationClip still;
		still.name = "Idle";
		still.duration = 1;
		still.tracks.resize(2);
		skin->clips = {std::move(sway), std::move(still)};
		std::vector<SkinInfluence> influences(original.Positions().size());
		for (std::size_t index = 0; index < influences.size(); ++index)
		{
			influences[index].joints[0] = original.Positions()[index].y > 0 ? 1u : 0u;
		}
		catalog.Register({AnimatedMeshId().Value(), "AnimatedCube",
			MeshAsset({original.Positions().begin(), original.Positions().end()}, {original.Indices().begin(), original.Indices().end()},
				{original.Texcoords().begin(), original.Texcoords().end()}, {original.Normals().begin(), original.Normals().end()},
				{original.Tangents().begin(), original.Tangents().end()}, {original.Submeshes().begin(), original.Submeshes().end()}, {},
				std::move(skin), std::move(influences))});
	}

	std::unique_ptr<KT::World::Scene> CreateAnimationDemoScene()
	{
		using namespace KT::World;
		SceneAsset asset;
		asset.name = "AnimationDemo";
		for (int index = 0; index < 5; ++index)
		{
			SceneObjectDefinition object;
			object.uuid = ObjectUuid::Generate();
			object.name = index == 4 ? "StaticCube" : "Actor" + std::to_string(index);
			object.components = {
				{"kt.MeshRenderer", 3,
					SceneValue::Object{{"mesh", (index == 4 ? KT::BuiltinAssets::CubeAssetId() : AnimatedMeshId()).ToString()},
						{"material", KT::BuiltinAssets::WhiteMaterialAssetId().ToString()}, {"visible", true},
						{"materials", SceneValue::Object{}}}},
				{"kt.LocalTransform", 1,
					SceneValue::Object{{"position", SceneValue::Array{(index - 2) * 1.7, 0.0, 0.0}},
						{"rotation", SceneValue::Array{0.0, 0.0, 0.0, 1.0}}, {"scale", SceneValue::Array{1.0, 2.0, 1.0}}}}};
			if (index != 4)
			{
				object.components.push_back({"kt.Animator", 1,
					SceneValue::Object{
						{"clip", "Sway"}, {"speed", 0.6 + index * 0.4}, {"loop", true}, {"playing", true}, {"enabled", true}}});
			}
			asset.objects.push_back(std::move(object));
		}
		SceneObjectDefinition camera;
		camera.uuid = ObjectUuid::Generate();
		camera.name = "Camera";
		camera.components = {
			{"kt.Camera", 1, SceneValue::Object{{"verticalFovRadians", 1.0471975512}, {"nearPlane", 0.1}, {"farPlane", 100.0}}},
			{"kt.LocalTransform", 1,
				SceneValue::Object{{"position", SceneValue::Array{0.0, 0.0, -12.0}}, {"rotation", SceneValue::Array{0.0, 0.0, 0.0, 1.0}},
					{"scale", SceneValue::Array{1.0, 1.0, 1.0}}}}};
		asset.objects.push_back(std::move(camera));
		return SceneLoader::Load(asset);
	}
}
