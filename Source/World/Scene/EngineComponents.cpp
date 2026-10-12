#include <World/Scene/ComponentRegistry.h>
#include <World/Scene/SceneComponents.h>
#include <World/Scene/ModelInstance.h>
#include <World/Physics/PhysicsSerialization.h>
#include <World/Animation/AnimatorSerialization.h>
#include <numbers>
#include <World/Lighting/DirectionalLight.h>

namespace KT::World
{
	// 同じ3要素形式を位置・scaleの保存と復元で共有する。
	static SceneValue EncodeVector(KT::Core::Math::Vector3 input)
	{
		return SceneValue::Array{double(input.x), double(input.y), double(input.z)};
	}

	static KT::Core::Math::Vector3 DecodeVector(const SceneValue& data)
	{
		const auto& values = data.AsArray();
		if (values.size() != 3)
		{
			throw std::invalid_argument("Vector3設定には3要素が必要です。");
		}
		return {values[0].AsFloat(), values[1].AsFloat(), values[2].AsFloat()};
	}

	void RegisterEngineComponents(ComponentRegistry& registry)
	{
		RegisterPhysicsComponents(registry);
		RegisterAnimatorComponent(registry);
		RegisterDirectionalLightComponent(registry);
		registry.Register<ActiveSelf>(
			"kt.ActiveSelf", 1, [](const ActiveSelf& input) -> SceneValue { return SceneValue::Object{{"value", input.value}}; },
			[](const SceneValue& data, const SceneReadContext&) -> ActiveSelf
			{
				data.RequireFields({"value"});
				return {data.At("value").AsBool()};
			});

		registry.Register<LocalTransform>(
			"kt.LocalTransform", 1,
			[](const LocalTransform& input) -> SceneValue
			{
				return SceneValue::Object{{"position", EncodeVector(input.position)}, {"scale", EncodeVector(input.scale)},
					{"rotation",
						SceneValue::Array{
							double(input.rotation.x), double(input.rotation.y), double(input.rotation.z), double(input.rotation.w)}}};
			},
			[](const SceneValue& data, const SceneReadContext&) -> LocalTransform
			{
				data.RequireFields({"position", "rotation", "scale"});
				const auto& rotation = data.At("rotation").AsArray();
				if (rotation.size() != 4)
				{
					throw std::invalid_argument("Quaternion設定にはxyzwの4要素が必要です。");
				}
				LocalTransform result;
				result.position = DecodeVector(data.At("position"));
				result.scale = DecodeVector(data.At("scale"));
				result.rotation = {rotation[0].AsFloat(), rotation[1].AsFloat(), rotation[2].AsFloat(), rotation[3].AsFloat()};
				if (result.rotation == KT::Core::Math::Quaternion{0, 0, 0, 0})
				{
					throw std::invalid_argument("Quaternion設定の全ゼロ値は回転に使えません。");
				}
				return result;
			});

		registry.Register<Camera>(
			"kt.Camera", 1,
			[](const Camera& input) -> SceneValue
			{
				return SceneValue::Object{{"verticalFovRadians", double(input.verticalFovRadians)}, {"nearPlane", double(input.nearPlane)},
					{"farPlane", double(input.farPlane)}};
			},
			[](const SceneValue& data, const SceneReadContext&) -> Camera
			{
				data.RequireFields({"verticalFovRadians", "nearPlane", "farPlane"});
				Camera result{data.At("verticalFovRadians").AsFloat(), data.At("nearPlane").AsFloat(), data.At("farPlane").AsFloat()};
				if (result.verticalFovRadians <= 0 || result.verticalFovRadians >= std::numbers::pi_v<float> || result.nearPlane <= 0 ||
					result.farPlane <= result.nearPlane)
				{
					throw std::invalid_argument("Cameraの視野角またはnear/far距離が不正です。");
				}
				return result;
			});

		// 保存はversion 3。旧version 2は単一Materialをfallbackとして明示変換する。
		ComponentDescriptor mesh;
		mesh.type = "kt.MeshRenderer";
		mesh.cppType = typeid(MeshRenderer);
		mesh.version = 3;
		mesh.capture = [](const World& world, Entity entity) -> SceneValue
		{
			const auto& input = world.GetComponent<MeshRenderer>(entity);
			SceneValue::Object materials;
			for (const auto& [slot, material] : input.materials)
			{
				materials.emplace(slot, material ? SceneValue(material->ToString()) : SceneValue{});
			}
			return SceneValue::Object{{"mesh", input.mesh ? SceneValue(input.mesh->ToString()) : SceneValue{}},
				{"material", input.material ? SceneValue(input.material->ToString()) : SceneValue{}}, {"visible", input.visible},
				{"materials", std::move(materials)}};
		};
		const auto decodeMesh = [](const SceneValue& data) -> MeshRenderer
		{
			data.RequireFields({"mesh", "material", "visible", "materials"});
			MeshRenderer result;
			if (!std::holds_alternative<std::nullptr_t>(data.At("mesh").value))
			{
				result.mesh = KT::Content::MeshAssetId::Parse(data.At("mesh").AsString());
			}
			if (!std::holds_alternative<std::nullptr_t>(data.At("material").value))
			{
				result.material = KT::Content::MaterialAssetId::Parse(data.At("material").AsString());
			}
			result.visible = data.At("visible").AsBool();
			for (const auto& [slot, material] : data.At("materials").AsObject())
			{
				if (slot.empty() || slot.size() > 256)
				{
					throw std::invalid_argument("MeshRendererのMaterial slot名は1..256byteが必要です。");
				}
				std::optional<KT::Content::MaterialAssetId> id;
				if (!std::holds_alternative<std::nullptr_t>(material.value))
				{
					id = KT::Content::MaterialAssetId::Parse(material.AsString());
				}
				result.materials.emplace(slot, id);
			}
			return result;
		};
		mesh.validate = [decodeMesh](const SceneValue& data, const SceneReadContext&) { (void)decodeMesh(data); };
		// Mesh/MaterialのUUIDはAsset参照であり、Sceneオブジェクト参照ではない。
		mesh.enumerateReferences = mesh.validate;
		mesh.restore = [decodeMesh](Scene& scene, EntityTarget entity, const SceneValue& data, const SceneReadContext&)
		{ scene.Commands().AddComponent<MeshRenderer>(entity, decodeMesh(data)); };
		mesh.migrate = [](std::uint32_t version, const SceneValue& data) -> SceneValue
		{
			if (version != 2)
			{
				throw std::invalid_argument("MeshRendererの自動互換読込はversion 2だけに対応しています。");
			}
			data.RequireFields({"mesh", "material", "visible"});
			auto result = data.AsObject();
			result.emplace("materials", SceneValue::Object{});
			return result;
		};
		registry.Register(std::move(mesh));
		// 過去の原本参照は現在の資源と切り離す。欠落した個体ノードは反映時に抑止する。
		registry.Register<ModelInstance>(
			"kt.ModelInstance", 1, [](const ModelInstance& input) { return input.Encode(); },
			[&registry](const SceneValue& data, const SceneReadContext& context)
			{
				auto result = ModelInstance::Decode(data);
				for (const auto& binding : result.nodes)
				{
					registry.Validate({"kt.LocalTransform", 1, binding.source.At("transform")}, context);
					if (!std::holds_alternative<std::nullptr_t>(binding.source.At("renderer").value))
					{
						registry.Validate({"kt.MeshRenderer", 3, binding.source.At("renderer")}, context);
					}
				}
				return result;
			},
			// 対応表は歴史情報であり、生存Sceneオブジェクトへの実参照ではない。
			[](const SceneValue&, const SceneReadContext&) {});
		// Scriptは検査と生成を分離し、全定義の検査成功までfactoryを呼ばない。
		ComponentDescriptor script;
		script.type = "kt.ScriptComponent";
		script.cppType = typeid(ScriptComponent);
		script.capture = [](const World& world, Entity entity)
		{
			std::vector<ScriptDefinition> definitions;
			for (const auto& entry : world.GetComponent<ScriptComponent>(entity).Entries())
			{
				definitions.push_back(entry.definition);
			}
			return ScriptRegistry::Encode(definitions);
		};
		script.validate = [](const SceneValue& data, const SceneReadContext& context)
		{
			for (const auto& definition : ScriptRegistry::Decode(data))
			{
				context.scripts.Validate(definition, context.objects);
			}
		};
		script.restore = [](Scene& scene, EntityTarget entity, const SceneValue& data, const SceneReadContext& context)
		{
			std::vector<ScriptEntry> entries;
			for (auto& definition : ScriptRegistry::Decode(data))
			{
				auto instance = context.scripts.Create(definition, context.objects);
				entries.push_back({std::move(definition), std::move(instance)});
			}
			scene.Commands().AddComponent<ScriptComponent>(entity, std::move(entries));
		};
		// Missing Scriptも型付きObjectUuidだけを列挙し、文字列設定と区別する。
		script.enumerateReferences = [](const SceneValue& data, const SceneReadContext& context)
		{
			for (const auto& definition : ScriptRegistry::Decode(data))
			{
				for (const auto& [name, setting] : definition.settings)
				{
					if (const auto* uuid = std::get_if<ObjectUuid>(&setting))
					{
						context.RequireReference(*uuid);
					}
				}
			}
		};
		registry.Register(std::move(script));
	}
}
