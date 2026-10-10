#include <World/Scene/SceneLoader.h>
#include <map>

namespace KT::World
{
	std::unique_ptr<Scene> SceneLoader::Load(const SceneAsset& asset, const ScriptFactory& scripts)
	{
		// 候補Sceneを別に作り、全成功するまで使用中のSceneへ触れない。
		auto scene = std::make_unique<Scene>(asset.name);
		std::map<ObjectUuid, DeferredEntity> reservations;
		for (const auto& object : asset.objects)
		{
			if (!object.uuid.IsValid() || reservations.contains(object.uuid))
			{
				throw std::invalid_argument("Scene定義のオブジェクトUUIDが空、または重複しています。");
			}
			reservations.emplace(object.uuid, scene->Commands().CreateEntity(object.name, object.uuid));
		}

		// 全Entityの生成を先に記録し、参照先の保存順序に依存しない。
		for (const auto& object : asset.objects)
		{
			const auto entity = reservations.at(object.uuid);
			if (object.active)
			{
				scene->Commands().AddComponent<ActiveSelf>(entity, *object.active);
			}
			if (object.transform)
			{
				scene->Commands().AddComponent<LocalTransform>(entity, *object.transform);
			}
			if (object.camera)
			{
				scene->Commands().AddComponent<Camera>(entity, *object.camera);
			}
			if (object.mesh)
			{
				scene->Commands().AddComponent<MeshRenderer>(entity, *object.mesh);
			}
			if (!object.scripts.empty())
			{
				std::vector<ScriptEntry> entries;
				entries.reserve(object.scripts.size());
				for (const auto& definition : object.scripts)
				{
					entries.push_back({definition, scripts ? scripts(definition) : nullptr});
				}
				scene->Commands().AddComponent<ScriptComponent>(entity, std::move(entries));
			}
			if (object.parent)
			{
				const auto parent = reservations.find(*object.parent);
				if (parent == reservations.end())
				{
					throw std::invalid_argument("Scene定義の親UUIDに対応するオブジェクトが同じScene内にありません。");
				}
				scene->Commands().SetParent(entity, parent->second);
			}
		}

		// 読込ではScript更新を行わず、構造とCPU入力だけを検証する。
		scene->commandResults_[0] = scene->commands_.Flush();
		if (!scene->commandResults_[0].Succeeded())
		{
			std::rethrow_exception(scene->commandResults_[0].error);
		}
		scene->updateNumber_ = 1;
		scene->snapshot_ = scene->ComputeCpu(scene->updateNumber_);
		return scene;
	}

	SceneAsset SceneLoader::Capture(const Scene& scene)
	{
		// 完成結果と予約の反映完了を確認し、途中のWorldを保存しない。
		(void)scene.GetSnapshot();
		if (scene.commands_.PendingCount() != 0)
		{
			throw std::logic_error("Sceneに未反映の予約コマンドがあるため保存用の定義を取得できません。");
		}

		// Entity参照をUUIDへ戻し、入力componentとScriptの設定だけをコピーする。
		SceneAsset asset;
		asset.name = scene.GetName();
		const auto& world = scene.GetWorld();
		for (const auto entity : world.Entities())
		{
			// 未対応componentを黙って捨てない。汎用Registryの保存窓口は後で追加する。
			for (const auto type : world.ComponentTypes(entity))
			{
				if (type != typeid(PersistentId) && type != typeid(Name) && type != typeid(Hierarchy) && type != typeid(ActiveSelf) &&
					type != typeid(LocalTransform) && type != typeid(Camera) && type != typeid(MeshRenderer) &&
					type != typeid(ScriptComponent))
				{
					throw std::logic_error("Sceneのcomponentに保存処理が未対応の型があるため保存用の定義を取得できません。");
				}
			}

			SceneObjectDefinition object;
			object.uuid = world.GetComponent<PersistentId>(entity).value;
			if (const auto* name = world.FindComponent<Name>(entity))
			{
				object.name = name->value;
			}
			if (const auto* hierarchy = world.FindComponent<Hierarchy>(entity); hierarchy && hierarchy->parent != Entity{})
			{
				object.parent = world.GetComponent<PersistentId>(hierarchy->parent).value;
			}
			if (const auto* active = world.FindComponent<ActiveSelf>(entity))
			{
				object.active = *active;
			}
			else
			{
				object.active.reset();
			}
			if (const auto* transform = world.FindComponent<LocalTransform>(entity))
			{
				object.transform = *transform;
			}
			else
			{
				object.transform.reset();
			}
			if (const auto* camera = world.FindComponent<Camera>(entity))
			{
				object.camera = *camera;
			}
			if (const auto* mesh = world.FindComponent<MeshRenderer>(entity))
			{
				object.mesh = *mesh;
			}
			if (const auto* scripts = world.FindComponent<ScriptComponent>(entity))
			{
				for (const auto& entry : scripts->Entries())
				{
					object.scripts.push_back(entry.definition);
				}
			}
			asset.objects.push_back(std::move(object));
		}
		return asset;
	}
}
