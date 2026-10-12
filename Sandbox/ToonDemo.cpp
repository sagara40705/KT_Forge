#include <ToonDemo.h>
#include <AnimationDemo.h>
#include <BuiltinAssets/StaticMeshAssets.h>
#include <World/Lighting/DirectionalLight.h>
#include <World/Animation/Animator.h>

namespace KT::Sandbox
{
	namespace
	{
		KT::Content::MaterialAssetId ActorMaterial()
		{
			return KT::Content::MaterialAssetId::Parse("4bc738c1-ce96-46ed-93bf-12caa49a2001");
		}

		KT::Content::MaterialAssetId FloorMaterial()
		{
			return KT::Content::MaterialAssetId::Parse("4bc738c1-ce96-46ed-93bf-12caa49a2002");
		}
	}

	void RegisterToonDemoAssets(KT::Content::AssetCatalog& catalog)
	{
		RegisterAnimationDemoAssets(catalog);
		catalog.Register(
			{ActorMaterial().Value(), "ToonActor", KT::Content::MaterialAsset({0.35f, 0.65f, 1, 1}, KT::Content::MaterialModel::Toon)});
		catalog.Register(
			{FloorMaterial().Value(), "ToonFloor", KT::Content::MaterialAsset({0.8f, 0.8f, 0.65f, 1}, KT::Content::MaterialModel::Toon)});
	}

	std::unique_ptr<KT::World::Scene> CreateToonDemoScene()
	{
		using namespace KT::World;
		auto scene = CreateAnimationDemoScene();
		auto& commands = scene->Commands();
		const auto light = commands.CreateEntity("MainLight");
		commands.AddComponent<DirectionalLight>(light, DirectionalLight{});
		const auto floor = commands.CreateEntity("Floor");
		commands.AddComponent<MeshRenderer>(floor, MeshRenderer{KT::BuiltinAssets::CubeAssetId(), FloorMaterial()});
		commands.AddComponent<LocalTransform>(floor, LocalTransform{{0, -1.3f, 3}, {}, {22, 0.3f, 18}});
		// 同じSkin素材でも再生速度と配置は個体ごとに持つ。
		for (unsigned index = 0; index < 15; ++index)
		{
			const auto actor = commands.CreateEntity("ExtraActor" + std::to_string(index));
			const bool skin = index % 3 != 0;
			const auto mesh =
				skin ? KT::Content::MeshAssetId::Parse("4bc738c1-ce96-46ed-93bf-12caa49a1001") : KT::BuiltinAssets::CubeAssetId();
			commands.AddComponent<MeshRenderer>(actor, MeshRenderer{mesh, ActorMaterial()});
			commands.AddComponent<LocalTransform>(
				actor, LocalTransform{{(int(index % 5) - 2) * 2.5f, 0, 3.0f + (index / 5) * 2.5f}, {}, {1, 2, 1}});
			if (skin)
			{
				Animator animator;
				animator.Play("Sway", true, 0.5 + index * 0.1);
				commands.AddComponent<Animator>(actor, std::move(animator));
			}
		}
		(void)scene->Update(0,
			[](Scene& current, const SceneUpdateContext&, double)
			{
				current.ForEachActive<MeshRenderer>(
					[](Entity, MeshRenderer& renderer)
					{
						// 白色Unlitの既存個体の一つを残し、共存経路を確認する。
						if (renderer.material == KT::BuiltinAssets::WhiteMaterialAssetId() &&
							renderer.mesh != KT::BuiltinAssets::CubeAssetId())
						{
							renderer.material = ActorMaterial();
						}
					});
				current.ForEachActive<Camera, LocalTransform>(
					[](Entity, Camera&, LocalTransform& transform)
					{
						transform.position = {0, 12, -18};
						transform.rotation = KT::Core::Math::FromAxisAngle({1, 0, 0}, 0.55f);
					});
			});
		return scene;
	}
}
