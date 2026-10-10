#include <World/Scene/SceneLoader.h>
#include <algorithm>

namespace KT::World
{
	void SceneLoader::Validate(const SceneAsset& asset, const ComponentRegistry& components, const ScriptRegistry& scripts)
	{
		if (!components.IsFrozen() || !scripts.IsFrozen())
		{
			throw std::logic_error("Scene読込・保存前に二つのRegistryをFreezeしてください。");
		}
		if (asset.version != 1)
		{
			throw std::invalid_argument("Sceneファイルのversionが未対応です。");
		}

		// 全IDを先に検査し、保存順に依存せず参照先と階層を検査する。
		ValidateSceneText(asset.name);
		std::set<ObjectUuid> objects;
		std::map<ObjectUuid, std::optional<ObjectUuid>> parents;
		for (const auto& object : asset.objects)
		{
			ValidateSceneText(object.name);
			if (!object.uuid.IsValid() || !objects.insert(object.uuid).second)
			{
				throw std::invalid_argument("SceneのオブジェクトUUIDが空、または重複しています。");
			}
			parents.emplace(object.uuid, object.parent);
		}
		for (const auto& object : asset.objects)
		{
			if (object.parent && (!object.parent->IsValid() || !objects.contains(*object.parent)))
			{
				throw std::invalid_argument("Sceneの親UUIDに対応するオブジェクトがありません: " + object.uuid.ToString());
			}
		}

		// 確定済み経路を共有し、深い階層でも再帰せず循環を検出する。
		std::set<ObjectUuid> completed;
		for (const auto& object : asset.objects)
		{
			std::set<ObjectUuid> path;
			auto current = std::optional<ObjectUuid>(object.uuid);
			while (current && !completed.contains(*current))
			{
				if (!path.insert(*current).second)
				{
					throw std::invalid_argument("Sceneの親子関係が循環しています: " + current->ToString());
				}
				current = parents.at(*current);
			}
			completed.insert(path.begin(), path.end());
		}

		// Component設定をすべて検査し、候補生成やScript factoryの実行と分離する。
		const SceneReadContext context{objects, scripts};
		for (const auto& object : asset.objects)
		{
			std::set<std::string> types;
			for (const auto& component : object.components)
			{
				if (!types.insert(component.type).second)
				{
					throw std::invalid_argument("SceneオブジェクトのComponent型が重複しています: " + component.type);
				}
				try
				{
					components.Validate(component, context);
				}
				catch (const std::invalid_argument& error)
				{
					throw std::invalid_argument("Sceneオブジェクト " + object.uuid.ToString() + " / " + error.what());
				}
			}
		}
	}

	std::unique_ptr<Scene> SceneLoader::Load(const SceneAsset& asset, const ComponentRegistry& components, const ScriptRegistry& scripts)
	{
		Validate(asset, components, scripts);
		auto scene = std::make_unique<Scene>(asset.name);
		std::map<ObjectUuid, DeferredEntity> reservations;
		std::set<ObjectUuid> objects;
		for (const auto& object : asset.objects)
		{
			reservations.emplace(object.uuid, scene->Commands().CreateEntity(object.name, object.uuid));
			objects.insert(object.uuid);
		}

		// 全生成の後にComponentを復元し、最後に親を設定する。
		const SceneReadContext context{objects, scripts};
		for (const auto& object : asset.objects)
		{
			const auto entity = reservations.at(object.uuid);
			for (const auto& component : object.components)
			{
				components.Find(component.type).restore(*scene, entity, component.data, context);
			}
		}
		for (const auto& object : asset.objects)
		{
			if (object.parent)
			{
				scene->Commands().SetParent(reservations.at(object.uuid), reservations.at(*object.parent));
			}
		}

		// 完成した候補だけを返す。読込中はScriptのゲーム更新を呼ばない。
		scene->commandResults_[0] = scene->commands_.Flush();
		if (!scene->commandResults_[0].Succeeded())
		{
			std::rethrow_exception(scene->commandResults_[0].error);
		}
		scene->updateNumber_ = 1;
		scene->snapshot_ = scene->ComputeCpu(scene->updateNumber_);
		return scene;
	}

	SceneAsset SceneLoader::Capture(const Scene& scene, const ComponentRegistry& components, const ScriptRegistry& scripts)
	{
		(void)scene.GetSnapshot();
		if (scene.commands_.PendingCount() != 0)
		{
			throw std::logic_error("Sceneに未反映の予約があるため保存できません。");
		}

		SceneAsset asset;
		asset.name = scene.GetName();
		const auto& world = scene.GetWorld();
		for (const auto entity : world.Entities())
		{
			SceneObjectDefinition object;
			object.uuid = world.GetComponent<PersistentId>(entity).value;
			object.name = world.GetComponent<Name>(entity).value;
			if (const auto* hierarchy = world.FindComponent<Hierarchy>(entity); hierarchy && hierarchy->parent != Entity{})
			{
				object.parent = world.GetComponent<PersistentId>(hierarchy->parent).value;
			}
			for (const auto type : world.ComponentTypes(entity))
			{
				if (type == typeid(PersistentId) || type == typeid(Name) || type == typeid(Hierarchy))
				{
					continue;
				}
				const auto& descriptor = components.Find(type);
				object.components.push_back({descriptor.type, descriptor.version, descriptor.capture(world, entity)});
			}
			std::sort(object.components.begin(), object.components.end(),
				[](const auto& first, const auto& second) { return first.type < second.type; });
			asset.objects.push_back(std::move(object));
		}
		std::sort(
			asset.objects.begin(), asset.objects.end(), [](const auto& first, const auto& second) { return first.uuid < second.uuid; });
		Validate(asset, components, scripts);
		return asset;
	}

	std::unique_ptr<Scene> SceneLoader::Load(const SceneAsset& asset)
	{
		ComponentRegistry components;
		RegisterEngineComponents(components);
		components.Freeze();
		ScriptRegistry scripts;
		scripts.Freeze();
		return Load(asset, components, scripts);
	}

	SceneAsset SceneLoader::Capture(const Scene& scene)
	{
		ComponentRegistry components;
		RegisterEngineComponents(components);
		components.Freeze();
		ScriptRegistry scripts;
		scripts.Freeze();
		return Capture(scene, components, scripts);
	}
}
