#include <World/Scene/SceneLoader.h>
#include <algorithm>
#include <unordered_map>

namespace KT::World
{
	namespace
	{
		// UUID全16バイトで検索し、衝突時はObjectUuidの完全一致で区別する。
		struct LoaderUuidHash
		{
			std::size_t operator()(const ObjectUuid& uuid) const noexcept
			{
				std::uint64_t hash = 14695981039346656037ULL;
				for (auto byte : uuid.bytes)
				{
					hash = (hash ^ byte) * 1099511628211ULL;
				}
				return static_cast<std::size_t>(hash);
			}
		};

		// ValidateとLoadが共有する一回の検証結果。親はasset配列のindexで解決済み。
		struct ResolvedScene
		{
			std::set<ObjectUuid> objects;
			std::vector<std::size_t> parents;
		};

		ResolvedScene ValidateAndResolve(const SceneAsset& asset, const ComponentRegistry& components, const ScriptRegistry& scripts)
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
			ResolvedScene resolved;
			std::unordered_map<ObjectUuid, std::size_t, LoaderUuidHash> nodes;
			nodes.reserve(asset.objects.size());
			resolved.parents.resize(asset.objects.size(), NoParent);
			for (std::size_t index = 0; index < asset.objects.size(); ++index)
			{
				const auto& object = asset.objects[index];
				ValidateSceneText(object.name);
				if (!object.uuid.IsValid() || !nodes.emplace(object.uuid, index).second)
				{
					throw std::invalid_argument("SceneのオブジェクトUUIDが空、または重複しています。");
				}
				resolved.objects.insert(object.uuid);
			}
			for (std::size_t index = 0; index < asset.objects.size(); ++index)
			{
				const auto& object = asset.objects[index];
				if (object.parent)
				{
					const auto parent = nodes.find(*object.parent);
					if (!object.parent->IsValid() || parent == nodes.end())
					{
						throw std::invalid_argument("Sceneの親UUIDに対応するオブジェクトがありません: " + object.uuid.ToString());
					}
					resolved.parents[index] = parent->second;
				}
			}

			// 未訪問・経路内・完了の三色で検査する。再帰・経路ごとのset確保を使わない。
			std::vector<unsigned char> colors(asset.objects.size(), 0);
			for (std::size_t start = 0; start < asset.objects.size(); ++start)
			{
				auto current = start;
				while (current != NoParent && colors[current] == 0)
				{
					colors[current] = 1;
					current = resolved.parents[current];
				}
				if (current != NoParent && colors[current] == 1)
				{
					throw std::invalid_argument("Sceneの親子関係が循環しています: " + asset.objects[current].uuid.ToString());
				}
				current = start;
				while (current != NoParent && colors[current] == 1)
				{
					colors[current] = 2;
					current = resolved.parents[current];
				}
			}
			// Component設定をすべて検査し、候補生成やScript factoryの実行と分離する。
			const SceneReadContext context{resolved.objects, scripts};
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
			return resolved;
		}
	}

	void SceneLoader::Validate(const SceneAsset& asset, const ComponentRegistry& components, const ScriptRegistry& scripts)
	{
		(void)ValidateAndResolve(asset, components, scripts);
	}

	std::unique_ptr<Scene> SceneLoader::Load(const SceneAsset& asset, const ComponentRegistry& components, const ScriptRegistry& scripts)
	{
		const auto resolved = ValidateAndResolve(asset, components, scripts);
		auto scene = std::make_unique<Scene>(asset.name);
		std::vector<DeferredEntity> reservations;
		reservations.reserve(asset.objects.size());
		for (const auto& object : asset.objects)
		{
			reservations.push_back(scene->Commands().CreateEntity(object.name, object.uuid));
		}

		// 検証で解決したindexをそのまま使い、生成後にUUIDを再索引しない。
		const SceneReadContext context{resolved.objects, scripts};
		for (std::size_t index = 0; index < asset.objects.size(); ++index)
		{
			for (const auto& component : asset.objects[index].components)
			{
				components.Find(component.type).restore(*scene, reservations[index], component.data, context);
			}
		}
		for (std::size_t index = 0; index < asset.objects.size(); ++index)
		{
			if (resolved.parents[index] != NoParent)
			{
				scene->Commands().SetParent(reservations[index], reservations[resolved.parents[index]]);
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
