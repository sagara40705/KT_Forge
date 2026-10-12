#include <RuntimeIntegration/ModelPlacement.h>
#include <RuntimeIntegration/ModelReconciler.h>
#include <World/Scene/SceneLoader.h>
#include <Core/Math/Matrix4.h>
#include <set>
#include <algorithm>

namespace KT::RuntimeIntegration
{
	namespace
	{
		KT::World::SceneComponentDefinition TransformDefinition(
			KT::Core::Math::Vector3 position, KT::Core::Math::Quaternion rotation, KT::Core::Math::Vector3 scale)
		{
			using Value = KT::World::SceneValue;
			return {"kt.LocalTransform", 1,
				Value::Object{{"position", Value::Array{double(position.x), double(position.y), double(position.z)}},
					{"rotation", Value::Array{double(rotation.x), double(rotation.y), double(rotation.z), double(rotation.w)}},
					{"scale", Value::Array{double(scale.x), double(scale.y), double(scale.z)}}}};
		}
	}

	KT::World::SceneAsset ModelPlacement::CreateScene(
		const KT::Content::ModelAsset& model, const std::map<std::string, KT::World::ObjectUuid>& previousIds)
	{
		model.Validate();
		KT::World::SceneAsset scene;
		scene.name = "ImportedModel";
		std::set<KT::World::ObjectUuid> ids;
		scene.objects.reserve(model.Nodes().size());
		for (const auto& node : model.Nodes())
		{
			KT::World::SceneObjectDefinition object;
			const auto previous = previousIds.find(node.key);
			object.uuid = previous == previousIds.end() ? KT::World::ObjectUuid::Generate() : previous->second;
			if (!object.uuid.IsValid() || !ids.insert(object.uuid).second)
			{
				throw std::invalid_argument("Model配置のObjectUuidが無効または重複しています。");
			}
			object.name = node.name;
			if (node.parent)
			{
				object.parent = scene.objects[*node.parent].uuid;
			}
			object.components.push_back(TransformDefinition(node.position, node.rotation, node.scale));
			if (node.mesh)
			{
				KT::World::SceneValue::Object materials;
				for (const auto& [slot, id] : node.materials)
				{
					materials.emplace(slot, id ? KT::World::SceneValue(id->ToString()) : KT::World::SceneValue{});
				}
				object.components.push_back({"kt.MeshRenderer", 3,
					KT::World::SceneValue::Object{{"mesh", node.mesh->ToString()},
						{"material", node.material ? KT::World::SceneValue(node.material->ToString()) : KT::World::SceneValue{}},
						{"visible", true}, {"materials", std::move(materials)}}});
			}
			scene.objects.push_back(std::move(object));
		}
		return scene;
	}

	ModelPlacementResult ModelPlacement::Append(KT::World::SceneAsset& scene, const KT::Content::AssetCatalog& catalog,
		KT::Content::ModelAssetId id, const KT::World::ComponentRegistry& components, const KT::World::ScriptRegistry& scripts,
		ModelPlacementOptions options)
	{
		// 参照検査と定義生成を先に終え、既存Sceneへの部分追加を防ぐ。
		const auto model = catalog.Resolve(std::optional(id));
		if (model.state != KT::Content::AssetReferenceState::Ready)
		{
			throw std::invalid_argument("配置するModel参照が欠落または種類違いです: " + id.ToString());
		}
		model.asset->ValidateReferences(catalog);
		KT::World::SceneLoader::Validate(scene, components, scripts);
		(void)KT::Core::Math::LocalMatrix(options.transform.position, options.transform.rotation, options.transform.scale);
		if (options.parent &&
			std::none_of(scene.objects.begin(), scene.objects.end(), [&](const auto& object) { return object.uuid == *options.parent; }))
		{
			throw std::invalid_argument("Modelの配置先の親ObjectUuidがScene内にありません。");
		}
		auto instance = CreateScene(*model.asset);
		ModelPlacementResult result{KT::World::ObjectUuid::Generate(), {}};
		KT::World::SceneObjectDefinition root{result.root, std::move(options.name), options.parent,
			{TransformDefinition(options.transform.position, options.transform.rotation, options.transform.scale)}};
		// Animatorはアプリが明示する。Skinのみの配置はbind poseで表示する。
		auto candidate = scene;
		candidate.objects.reserve(candidate.objects.size() + instance.objects.size() + 1);
		candidate.objects.push_back(std::move(root));
		for (std::size_t index = 0; index < instance.objects.size(); ++index)
		{
			auto& object = instance.objects[index];
			result.nodes.emplace(model.asset->Nodes()[index].key, object.uuid);
			if (!object.parent)
			{
				object.parent = result.root;
			}
			candidate.objects.push_back(std::move(object));
		}
		// UUID衝突・Component契約まで検査した後、確保を伴わないswapで公開する。
		KT::World::SceneLoader::Validate(candidate, components, scripts);
		std::map<KT::Content::ModelNodeId, KT::World::ObjectUuid> bindings;
		for (const auto& node : model.asset->Nodes())
		{
			bindings.emplace(node.id, result.nodes.at(node.key));
		}
		ModelReconciler::Attach(candidate, catalog, id, result.root, bindings, components, scripts);
		scene.objects.swap(candidate.objects);
		return result;
	}
}
