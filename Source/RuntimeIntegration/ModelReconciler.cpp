#include <RuntimeIntegration/ModelReconciler.h>
#include <RuntimeIntegration/ModelPlacement.h>
#include <Content/ModelRevision.h>
#include <World/Scene/SceneLoader.h>
#include <World/Scene/TransformMath.h>
#include <algorithm>

namespace KT::RuntimeIntegration
{
	namespace
	{
		using namespace KT::World;
		using namespace KT::Content;
		using Objects = std::map<ObjectUuid, SceneObjectDefinition>;

		SceneComponentDefinition* Find(SceneObjectDefinition& object, std::string_view type)
		{
			const auto found = std::find_if(
				object.components.begin(), object.components.end(), [&](const auto& component) { return component.type == type; });
			return found == object.components.end() ? nullptr : &*found;
		}

		SceneValue Data(const SceneObjectDefinition& object, std::string_view type)
		{
			const auto component =
				std::find_if(object.components.begin(), object.components.end(), [&](const auto& entry) { return entry.type == type; });
			return component == object.components.end() ? SceneValue{} : component->data;
		}

		bool Null(const SceneValue& value)
		{
			return std::holds_alternative<std::nullptr_t>(value.value);
		}

		SceneValue Source(const ModelAsset& model, std::size_t index, const SceneObjectDefinition& definition)
		{
			const auto& node = model.Nodes()[index];
			return SceneValue::Object{{"name", node.name},
				{"parent", node.parent ? SceneValue(model.Nodes()[*node.parent].id.ToString()) : SceneValue{}},
				{"transform", Data(definition, "kt.LocalTransform")}, {"renderer", Data(definition, "kt.MeshRenderer")}};
		}

		std::optional<ObjectUuid> SourceParent(const SceneValue& source, const std::map<ModelNodeId, ObjectUuid>& mapping, ObjectUuid root)
		{
			const auto& value = source.At("parent");
			return Null(value) ? root : mapping.at(ModelNodeId::Parse(value.AsString()));
		}

		void Store(SceneObjectDefinition& object, std::string type, std::uint32_t version, SceneValue data)
		{
			if (auto* existing = Find(object, type))
			{
				existing->version = version;
				existing->data = std::move(data);
			}
			else
			{
				object.components.push_back({std::move(type), version, std::move(data)});
			}
		}

		std::optional<SceneValue> Field(const SceneValue::Object& fields, const std::string& key)
		{
			const auto found = fields.find(key);
			return found == fields.end() ? std::nullopt : std::optional(found->second);
		}

		// 不在と明示nullを区別し、同値の明示overrideも保持する。
		void Merge(SceneValue::Object& current, const SceneValue::Object& oldSource, const SceneValue::Object& newSource,
			const std::string& key, const std::string& field, ModelNodeBinding& binding, const ModelNodeResolution& resolution)
		{
			const bool follow = resolution.followFields.contains(field);
			if (follow)
			{
				binding.overrides.erase(field);
			}
			if (!follow && (binding.overrides.contains(field) || Field(current, key) != Field(oldSource, key)))
			{
				binding.overrides.insert(field);
				return;
			}
			const auto source = newSource.find(key);
			if (source == newSource.end())
			{
				current.erase(key);
			}
			else
			{
				current.insert_or_assign(key, source->second);
			}
		}

		KT::Core::Math::Matrix4 WorldMatrix(const Objects& objects, ObjectUuid object)
		{
			std::vector<ObjectUuid> chain;
			std::set<ObjectUuid> seen;
			std::optional<ObjectUuid> cursor = object;
			while (cursor)
			{
				if (!seen.insert(*cursor).second)
				{
					throw std::invalid_argument("反映候補の親参照が循環しています。");
				}
				chain.push_back(*cursor);
				cursor = objects.at(*cursor).parent;
			}
			auto result = KT::Core::Math::Identity();
			for (auto iterator = chain.rbegin(); iterator != chain.rend(); ++iterator)
			{
				const auto data = Data(objects.at(*iterator), "kt.LocalTransform");
				if (Null(data))
				{
					continue;
				}
				const auto& position = data.At("position").AsArray();
				const auto& rotation = data.At("rotation").AsArray();
				const auto& scale = data.At("scale").AsArray();
				result = KT::Core::Math::LocalMatrix({position[0].AsFloat(), position[1].AsFloat(), position[2].AsFloat()},
							 {rotation[0].AsFloat(), rotation[1].AsFloat(), rotation[2].AsFloat(), rotation[3].AsFloat()},
							 {scale[0].AsFloat(), scale[1].AsFloat(), scale[2].AsFloat()}) *
					result;
			}
			return result;
		}

		SceneValue TransformValue(LocalTransform transform)
		{
			return SceneValue::Object{
				{"position", SceneValue::Array{double(transform.position.x), double(transform.position.y), double(transform.position.z)}},
				{"rotation",
					SceneValue::Array{double(transform.rotation.x), double(transform.rotation.y), double(transform.rotation.z),
						double(transform.rotation.w)}},
				{"scale", SceneValue::Array{double(transform.scale.x), double(transform.scale.y), double(transform.scale.z)}}};
		}

		// 有効な参照は非表示・未使用slotも検査し、失効したoverrideを黙って消さない。
		void ValidateCandidate(
			const SceneAsset& scene, const AssetCatalog& catalog, const ComponentRegistry& components, const ScriptRegistry& scripts)
		{
			SceneLoader::Validate(scene, components, scripts);
			auto cpu = scene;
			for (auto& object : cpu.objects)
			{
				// 検証用コピーだけ互換変換し、非対象個体の保存値は変更しない。
				for (auto& component : object.components)
				{
					if (auto migrated = components.ReadLegacyData(component))
					{
						component.data = std::move(*migrated);
						component.version = components.Find(component.type).version;
					}
				}
				const auto renderer = Data(object, "kt.MeshRenderer");
				if (!Null(renderer))
				{
					std::vector<SceneValue> materials{renderer.At("material")};
					for (const auto& [slot, material] : renderer.At("materials").AsObject())
					{
						materials.push_back(material);
					}
					for (const auto& value : materials)
					{
						if (!Null(value))
						{
							const auto material = catalog.Resolve(std::optional(MaterialAssetId::Parse(value.AsString())));
							if (material.state != AssetReferenceState::Ready ||
								(material.asset->Settings().baseColorTexture &&
									catalog.Resolve(material.asset->Settings().baseColorTexture).state != AssetReferenceState::Ready))
							{
								throw std::invalid_argument(
									"反映候補のMaterialまたはTexture参照が欠落・種類違いです: " + object.uuid.ToString());
							}
						}
					}
					if (!Null(renderer.At("mesh")))
					{
						ModelNode node;
						node.key = object.uuid.ToString();
						node.mesh = MeshAssetId::Parse(renderer.At("mesh").AsString());
						if (!Null(renderer.At("material")))
						{
							node.material = MaterialAssetId::Parse(renderer.At("material").AsString());
						}
						for (const auto& [slot, material] : renderer.At("materials").AsObject())
						{
							node.materials.emplace(
								slot, Null(material) ? std::nullopt : std::optional(MaterialAssetId::Parse(material.AsString())));
						}
						ModelAsset({std::move(node)}).ValidateReferences(catalog);
					}
				}
				// CPU評価には組込み値だけを渡し、Script/custom Componentのfactoryを呼ばない。
				std::erase_if(object.components,
					[](const auto& component)
					{
						return component.type != "kt.LocalTransform" && component.type != "kt.ActiveSelf" &&
							component.type != "kt.Camera" && component.type != "kt.MeshRenderer";
					});
			}
			(void)SceneLoader::Load(cpu);
		}
	}

	const KT::World::SceneAsset& ModelReconcilePlan::Preview() const noexcept
	{
		return candidate_;
	}

	std::span<const ModelReconcileConflict> ModelReconcilePlan::Conflicts() const noexcept
	{
		return conflicts_;
	}

	void ModelReconciler::Attach(SceneAsset& scene, const AssetCatalog& catalog, ModelAssetId model, ObjectUuid root,
		const std::map<ModelNodeId, ObjectUuid>& objects, const ComponentRegistry& components, const ScriptRegistry& scripts)
	{
		SceneLoader::Validate(scene, components, scripts);
		const auto lookup = catalog.Resolve(std::optional(model));
		if (lookup.state != AssetReferenceState::Ready)
		{
			throw std::invalid_argument("リンクする旧Model原本が欠落または種類違いです。");
		}
		lookup.asset->ValidateReferences(catalog);
		if (objects.size() != lookup.asset->Nodes().size())
		{
			throw std::invalid_argument("旧Modelの全ノードとObjectUuidの明示対応が必要です。");
		}
		auto candidate = scene;
		const auto rootObject =
			std::find_if(candidate.objects.begin(), candidate.objects.end(), [&](const auto& object) { return object.uuid == root; });
		if (rootObject == candidate.objects.end() || Find(*rootObject, "kt.ModelInstance"))
		{
			throw std::invalid_argument("リンク先の配置rootがないか、既にModelInstanceを持っています。");
		}
		auto definition = ModelPlacement::CreateScene(*lookup.asset);
		ModelInstance instance{model, ModelRevision(catalog, model), {}};
		std::set<ObjectUuid> unique;
		for (std::size_t index = 0; index < lookup.asset->Nodes().size(); ++index)
		{
			const auto id = lookup.asset->Nodes()[index].id;
			const auto mapping = objects.find(id);
			if (mapping == objects.end() || mapping->second == root || !unique.insert(mapping->second).second ||
				std::none_of(
					scene.objects.begin(), scene.objects.end(), [&](const auto& object) { return object.uuid == mapping->second; }))
			{
				throw std::invalid_argument("Modelリンクの明示対応が欠落・重複・root自身を参照しています。");
			}
			instance.nodes.push_back(
				{id, mapping->second, ModelNodeState::Present, Source(*lookup.asset, index, definition.objects[index]), {}});
		}
		rootObject->components.push_back({"kt.ModelInstance", 1, instance.Encode()});
		SceneLoader::Validate(candidate, components, scripts);
		scene.objects.swap(candidate.objects);
	}

	void ModelReconciler::SetOverride(SceneAsset& scene, ObjectUuid root, ModelNodeId node, std::string field)
	{
		ModelInstance::ValidateOverride(field);
		auto candidate = scene;
		const auto object =
			std::find_if(candidate.objects.begin(), candidate.objects.end(), [&](const auto& entry) { return entry.uuid == root; });
		if (object == candidate.objects.end() || !Find(*object, "kt.ModelInstance"))
		{
			throw std::invalid_argument("override指定先のModelInstanceがありません。");
		}
		auto* component = Find(*object, "kt.ModelInstance");
		auto instance = ModelInstance::Decode(component->data);
		const auto binding =
			std::find_if(instance.nodes.begin(), instance.nodes.end(), [&](const auto& entry) { return entry.node == node; });
		if (binding == instance.nodes.end() || binding->state != ModelNodeState::Present)
		{
			throw std::invalid_argument("override指定先のModelノードがありません。");
		}
		binding->overrides.insert(std::move(field));
		component->data = instance.Encode();
		scene.objects.swap(candidate.objects);
	}

	ModelReconcilePlan ModelReconciler::Plan(const SceneAsset& scene, const AssetCatalog& catalog, const ComponentRegistry& components,
		const ScriptRegistry& scripts, const ModelReconcileOptions& options, const ModelReconcileSelection& selection)
	{
		SceneLoader::Validate(scene, components, scripts);
		if (!catalog.IsFrozen())
		{
			throw std::logic_error("Model反映前に新しいAssetCatalogをFreezeしてください。");
		}
		ModelReconcilePlan plan;
		plan.before_ = scene;
		plan.candidate_ = scene;
		Objects objects;
		std::vector<ObjectUuid> order;
		for (const auto& object : scene.objects)
		{
			objects.emplace(object.uuid, object);
			order.push_back(object.uuid);
		}
		const auto original = objects;
		// 非対象個体も所有権を検査し、対象個体から他個体のオブジェクトを変更しない。
		std::set<ObjectUuid> owned;
		std::set<ObjectUuid> referenceObjects;
		std::set<ObjectUuid> availableRoots;
		std::set<ModelAssetId> availableModels;
		std::set<ObjectUuid> selectedRoots;
		for (const auto& object : scene.objects)
		{
			referenceObjects.insert(object.uuid);
			const auto data = Data(object, "kt.ModelInstance");
			if (Null(data))
			{
				continue;
			}
			const auto instance = ModelInstance::Decode(data);
			availableRoots.insert(object.uuid);
			availableModels.insert(instance.model);
			if ((selection.roots.empty() || selection.roots.contains(object.uuid)) &&
				(selection.models.empty() || selection.models.contains(instance.model)))
			{
				selectedRoots.insert(object.uuid);
			}
			for (const auto& binding : instance.nodes)
			{
				if (binding.object == object.uuid || (binding.state != ModelNodeState::Detached && !owned.insert(binding.object).second))
				{
					throw std::invalid_argument("Model個体間のObjectUuid対応が重複またはroot自身です。");
				}
			}
		}
		for (const auto root : selection.roots)
		{
			if (!availableRoots.contains(root))
			{
				throw std::invalid_argument("Model反映の対象rootがSceneにありません: " + root.ToString());
			}
		}
		for (const auto root : availableRoots)
		{
			if (owned.contains(root))
			{
				throw std::invalid_argument("Model配置rootを別個体の管理ノードとして所有できません。");
			}
		}
		for (const auto model : selection.models)
		{
			if (!availableModels.contains(model))
			{
				throw std::invalid_argument("Model反映の対象ModelがSceneに配置されていません: " + model.ToString());
			}
		}
		if ((!selection.roots.empty() || !selection.models.empty()) && selectedRoots.empty())
		{
			throw std::invalid_argument("Modelとrootの反映条件に一致する個体がありません。");
		}
		const SceneReadContext referenceContext{referenceObjects, scripts};
		std::set<std::pair<ObjectUuid, ModelNodeId>> usedOptions;
		for (const auto& rootDefinition : scene.objects)
		{
			const auto data = Data(rootDefinition, "kt.ModelInstance");
			if (Null(data) || !selectedRoots.contains(rootDefinition.uuid))
			{
				continue;
			}
			const auto root = rootDefinition.uuid;
			const auto conflict = [&](std::optional<ModelNodeId> node, std::string reason)
			{ plan.conflicts_.push_back({root, node, std::move(reason)}); };
			try
			{
				auto instance = ModelInstance::Decode(data);
				const auto lookup = catalog.Resolve(std::optional(instance.model));
				if (lookup.state != AssetReferenceState::Ready)
				{
					throw std::invalid_argument("リンク先Modelが新Catalogにありません。");
				}
				const auto revision = ModelRevision(catalog, instance.model);
				const auto definition = ModelPlacement::CreateScene(*lookup.asset);
				std::map<ModelNodeId, ObjectUuid> mapping;
				std::map<ModelNodeId, std::size_t> bindings;
				std::map<ModelNodeId, SceneValue> sources;
				for (std::size_t index = 0; index < instance.nodes.size(); ++index)
				{
					auto& binding = instance.nodes[index];
					mapping.emplace(binding.node, binding.object);
					bindings.emplace(binding.node, index);
					if (binding.state == ModelNodeState::Present && !objects.contains(binding.object))
					{
						binding.state = ModelNodeState::Suppressed;
					}
					// 対象の管理ノードだけ互換変換し、非対象とdetach済みの定義を保持する。
					if (binding.state == ModelNodeState::Present)
					{
						for (auto& component : objects.at(binding.object).components)
						{
							if (auto migrated = components.ReadLegacyData(component))
							{
								component.data = std::move(*migrated);
								component.version = components.Find(component.type).version;
							}
						}
					}
					if (const auto option = options.find({root, binding.node}); option != options.end())
					{
						usedOptions.insert(option->first);
						if (option->second.restore && (option->second.detach || binding.state != ModelNodeState::Suppressed))
						{
							throw std::invalid_argument("復元は抑止ノードだけに指定でき、detachと同時には指定できません。");
						}
						for (const auto& field : option->second.followFields)
						{
							ModelInstance::ValidateOverride(field);
						}
					}
				}
				// 現在の部分木をdetachする。子の管理リンクも切ってから構造差分を扱う。
				std::set<ObjectUuid> detached;
				for (const auto& binding : instance.nodes)
				{
					if (const auto option = options.find({root, binding.node}); option != options.end() && option->second.detach)
					{
						detached.insert(binding.object);
					}
				}
				bool expanded = true;
				while (expanded)
				{
					expanded = false;
					for (const auto& [id, object] : objects)
					{
						if (object.parent && detached.contains(*object.parent))
						{
							expanded = detached.insert(id).second || expanded;
						}
					}
				}
				for (auto& binding : instance.nodes)
				{
					if (detached.contains(binding.object))
					{
						binding.state = ModelNodeState::Detached;
					}
				}
				// 復元対象の原本部分木を親先行順で広げ、消えた原本への復元指定を拒否する。
				std::set<ModelNodeId> restored;
				for (const auto& node : lookup.asset->Nodes())
				{
					const auto option = options.find({root, node.id});
					if ((option != options.end() && option->second.restore) ||
						(node.parent && restored.contains(lookup.asset->Nodes()[*node.parent].id)))
					{
						restored.insert(node.id);
					}
				}
				for (const auto& binding : instance.nodes)
				{
					const auto option = options.find({root, binding.node});
					if (option != options.end() && option->second.restore && !restored.contains(binding.node))
					{
						throw std::invalid_argument("復元指定したノードが新Model原本にありません。");
					}
				}
				// 新規・復元ノードは原本順に追加し、抑止・detachした親の下では生成しない。
				for (std::size_t index = 0; index < lookup.asset->Nodes().size(); ++index)
				{
					const auto& node = lookup.asset->Nodes()[index];
					const auto source = Source(*lookup.asset, index, definition.objects[index]);
					sources.emplace(node.id, source);
					if (!bindings.contains(node.id))
					{
						auto object = definition.objects[index];
						while (objects.contains(object.uuid) || owned.contains(object.uuid))
						{
							object.uuid = ObjectUuid::Generate();
						}
						const auto state = node.parent &&
								instance.nodes[bindings.at(lookup.asset->Nodes()[*node.parent].id)].state != ModelNodeState::Present ?
							ModelNodeState::Suppressed :
							ModelNodeState::Present;
						mapping.emplace(node.id, object.uuid);
						bindings.emplace(node.id, instance.nodes.size());
						instance.nodes.push_back({node.id, object.uuid, state, source, {}});
						owned.insert(object.uuid);
						if (state == ModelNodeState::Present)
						{
							object.parent = SourceParent(source, mapping, root);
							order.push_back(object.uuid);
							objects.emplace(object.uuid, std::move(object));
						}
					}
					else if (restored.contains(node.id))
					{
						auto& binding = instance.nodes[bindings.at(node.id)];
						if (binding.state == ModelNodeState::Detached)
						{
							throw std::invalid_argument("復元部分木にdetach済みのノードがあります。リンクを自動で奪えません。");
						}
						if (binding.state == ModelNodeState::Suppressed)
						{
							if (objects.contains(binding.object) ||
								(node.parent &&
									instance.nodes[bindings.at(lookup.asset->Nodes()[*node.parent].id)].state != ModelNodeState::Present))
							{
								throw std::invalid_argument("抑止ノードのUUIDが既に使用されているか、原本の親が抑止・detach状態です。");
							}
							auto object = definition.objects[index];
							object.uuid = binding.object;
							object.parent = SourceParent(source, mapping, root);
							binding.state = ModelNodeState::Present;
							binding.source = source;
							binding.overrides.clear();
							order.push_back(object.uuid);
							objects.emplace(object.uuid, std::move(object));
						}
					}
				}
				// 三者比較はフィールド単位。配置rootの名前・親・TRSへは触れない。
				for (const auto& node : lookup.asset->Nodes())
				{
					auto& binding = instance.nodes[bindings.at(node.id)];
					const auto& source = sources.at(node.id);
					if (binding.state != ModelNodeState::Present)
					{
						binding.source = source;
						continue;
					}
					auto& object = objects.at(binding.object);
					const auto option = options.find({root, node.id});
					const auto resolution = option == options.end() ? ModelNodeResolution{} : option->second;
					auto* transform = Find(object, "kt.LocalTransform");
					if (!transform)
					{
						conflict(node.id, "管理するLocalTransformが個体から削除されています。detachで保持してください。");
						continue;
					}
					auto local = transform->data.AsObject();
					bool transformOverride = false;
					for (const auto* field : {"position", "rotation", "scale"})
					{
						transformOverride = transformOverride ||
							(!resolution.followFields.contains(field) &&
								(binding.overrides.contains(field) || local.at(field) != binding.source.At("transform").At(field)));
						Merge(local, binding.source.At("transform").AsObject(), source.At("transform").AsObject(), field, field, binding,
							resolution);
					}
					const auto oldParent = SourceParent(binding.source, mapping, root);
					const auto newParent = SourceParent(source, mapping, root);
					const bool parentOverride = binding.overrides.contains("parent") || object.parent != oldParent;
					const bool parentChanged = oldParent != newParent;
					if (parentChanged && (transformOverride || parentOverride) && resolution.parent == ModelParentResolution::Ask &&
						!resolution.followFields.contains("parent"))
					{
						conflict(node.id,
							"原本の親変更と個体Transform/親編集が競合しています。KeepLocal・KeepWorld・FollowSourceを指定してください。");
					}
					else if (resolution.parent == ModelParentResolution::KeepWorld && parentChanged)
					{
						const auto world = WorldMatrix(original, binding.object);
						const auto parentWorld = newParent ? WorldMatrix(objects, *newParent) : KT::Core::Math::Identity();
						const auto retained = DecomposeLocalTransform(world * KT::Core::Math::Inverse(parentWorld));
						if (!TransformNear(
								world, KT::Core::Math::LocalMatrix(retained.position, retained.rotation, retained.scale) * parentWorld))
						{
							throw std::invalid_argument("KeepWorldの反映候補でWorld行列を保持できません。");
						}
						const auto retainedData = TransformValue(retained);
						local = retainedData.AsObject();
						object.parent = newParent;
						binding.overrides.erase("parent");
						binding.overrides.insert({"position", "rotation", "scale"});
					}
					else if (resolution.parent == ModelParentResolution::FollowSource || resolution.followFields.contains("parent"))
					{
						object.parent = newParent;
						binding.overrides.erase("parent");
						if (resolution.parent == ModelParentResolution::FollowSource)
						{
							local = source.At("transform").AsObject();
							for (const auto* field : {"position", "rotation", "scale"})
							{
								binding.overrides.erase(field);
							}
						}
					}
					else if (!parentOverride || resolution.parent == ModelParentResolution::KeepLocal)
					{
						object.parent = newParent;
						binding.overrides.erase("parent");
						if (parentChanged && resolution.parent == ModelParentResolution::KeepLocal)
						{
							local = transform->data.AsObject();
							for (const auto* field : {"position", "rotation", "scale"})
							{
								if (local.at(field) != source.At("transform").At(field))
								{
									binding.overrides.insert(field);
								}
							}
						}
					}
					else
					{
						binding.overrides.insert("parent");
					}
					Store(object, "kt.LocalTransform", 1, std::move(local));
					SceneValue::Object name{{"name", object.name}};
					Merge(name, binding.source.AsObject(), source.AsObject(), "name", "name", binding, resolution);
					object.name = name.at("name").AsString();
					const auto previous = binding.source.At("renderer");
					const auto next = source.At("renderer");
					const auto current = Data(object, "kt.MeshRenderer");
					if (!Null(previous) && !Null(next) && !Null(current))
					{
						auto fields = current.AsObject();
						for (const auto* field : {"mesh", "material", "visible"})
						{
							Merge(fields, previous.AsObject(), next.AsObject(), field, field, binding, resolution);
						}
						const auto& oldMaterials = previous.At("materials").AsObject();
						const auto& newMaterials = next.At("materials").AsObject();
						auto assignments = current.At("materials").AsObject();
						std::set<std::string> slots;
						const std::array<const SceneValue::Object*, 3> materialSets{&oldMaterials, &newMaterials, &assignments};
						for (const auto* values : materialSets)
						{
							for (const auto& [slot, material] : *values)
							{
								slots.insert(slot);
							}
						}
						// 同じMesh UUIDのslot構成変更も、新Catalogの実体で照合する。
						std::set<std::string> meshSlots;
						if (!Null(fields.at("mesh")))
						{
							const auto mesh = catalog.Resolve(std::optional(MeshAssetId::Parse(fields.at("mesh").AsString())));
							if (mesh.state == AssetReferenceState::Ready)
							{
								for (const auto& submesh : mesh.asset->Submeshes())
								{
									meshSlots.insert(submesh.materialSlot);
								}
							}
						}
						for (const auto& slot : slots)
						{
							const auto field = "materials:" + slot;
							Merge(assignments, oldMaterials, newMaterials, slot, field, binding, resolution);
							if (!meshSlots.contains(slot))
							{
								if (binding.overrides.contains(field))
								{
									conflict(node.id, "個体が変更したMaterial slotが新Meshにありません: " + slot);
								}
								else
								{
									assignments.erase(slot);
								}
							}
						}
						fields["materials"] = std::move(assignments);
						Store(object, "kt.MeshRenderer", 3, std::move(fields));
					}
					else if (previous != next)
					{
						const bool rendererOverride = std::any_of(binding.overrides.begin(), binding.overrides.end(), [](const auto& field)
							{ return field == "mesh" || field == "material" || field == "visible" || field.starts_with("materials:"); });
						if (current != previous || rendererOverride)
						{
							conflict(node.id, "MeshRendererの追加・削除と個体編集が競合しています。detachで保持してください。");
						}
						else if (Null(next))
						{
							std::erase_if(object.components, [](const auto& component) { return component.type == "kt.MeshRenderer"; });
						}
						else
						{
							Store(object, "kt.MeshRenderer", 3, next);
						}
					}
					else if (!Null(previous) && Null(current))
					{
						conflict(node.id, "管理するMeshRendererが個体から削除されています。detachで保持してください。");
					}
					binding.source = source;
				}
				// 原本で消えたノードは、現在値・追加Component・親子・外部参照を確認する。
				std::set<ObjectUuid> removed;
				for (auto& binding : instance.nodes)
				{
					if (sources.contains(binding.node) || binding.state != ModelNodeState::Present)
					{
						continue;
					}
					const auto& object = objects.at(binding.object);
					const bool custom = std::any_of(object.components.begin(), object.components.end(),
						[](const auto& component) { return component.type != "kt.LocalTransform" && component.type != "kt.MeshRenderer"; });
					if (custom || !binding.overrides.empty() || object.name != binding.source.At("name").AsString() ||
						object.parent != SourceParent(binding.source, mapping, root) ||
						Data(object, "kt.LocalTransform") != binding.source.At("transform") ||
						Data(object, "kt.MeshRenderer") != binding.source.At("renderer"))
					{
						conflict(binding.node, "原本で削除されたノードに個体編集または追加Componentがあります。detachで保持してください。");
						continue;
					}
					removed.insert(binding.object);
				}
				for (const auto& [candidateId, candidate] : objects)
				{
					referenceObjects.insert(candidateId);
				}
				for (const auto& [id, object] : objects)
				{
					if (removed.contains(id))
					{
						continue;
					}
					if (object.parent && removed.contains(*object.parent))
					{
						conflict({}, "削除予定ノードに保持対象の子があります: " + id.ToString());
					}
					for (const auto& component : object.components)
					{
						if (removed.empty())
						{
							continue;
						}
						// 原Sceneからの参照を調べ、新規生成したオブジェクトのUUIDも許可する。
						const auto references = components.References(component, referenceContext);
						if (!references)
						{
							conflict(
								{}, "Componentの参照列挙契約が未定義のため削除を保護します: " + id.ToString() + " / " + component.type);
						}
						else if (std::any_of(
									 references->begin(), references->end(), [&](ObjectUuid uuid) { return removed.contains(uuid); }))
						{
							conflict({}, "削除予定ノードへの外部Component参照があります: " + id.ToString() + " / " + component.type);
						}
					}
				}
				for (auto& binding : instance.nodes)
				{
					if (removed.contains(binding.object))
					{
						objects.erase(binding.object);
						binding.state = ModelNodeState::Suppressed;
					}
				}
				instance.revision = revision;
				Store(objects.at(root), "kt.ModelInstance", 1, instance.Encode());
			}
			catch (const std::exception& error)
			{
				conflict({}, "Model個体の反映候補を生成できません: " + std::string(error.what()));
			}
		}
		for (const auto& [key, resolution] : options)
		{
			if (!usedOptions.contains(key))
			{
				throw std::invalid_argument("Model競合解決の対象rootまたはノードがありません。");
			}
		}
		plan.candidate_.objects.clear();
		for (const auto id : order)
		{
			if (objects.contains(id))
			{
				plan.candidate_.objects.push_back(std::move(objects.at(id)));
			}
		}
		if (plan.conflicts_.empty())
		{
			try
			{
				ValidateCandidate(plan.candidate_, catalog, components, scripts);
			}
			catch (const std::exception& error)
			{
				plan.conflicts_.push_back({{}, {}, "Scene全体の反映候補が無効です: " + std::string(error.what())});
			}
		}
		return plan;
	}

	void ModelReconciler::Apply(
		SceneAsset& scene, const ModelReconcilePlan& plan, const ComponentRegistry& components, const ScriptRegistry& scripts)
	{
		if (!plan.conflicts_.empty())
		{
			throw std::invalid_argument("未解決のModel反映競合があるためSceneを変更できません。");
		}
		if (scene != plan.before_)
		{
			throw std::invalid_argument("Model反映計画の作成後にSceneが変更されています。Planを作り直してください。");
		}
		auto candidate = plan.candidate_;
		SceneLoader::Validate(candidate, components, scripts);
		scene.objects.swap(candidate.objects);
	}
}
