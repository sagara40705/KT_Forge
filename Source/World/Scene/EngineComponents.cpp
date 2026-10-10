#include <World/Scene/ComponentRegistry.h>
#include <World/Scene/SceneComponents.h>
#include <numbers>

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

		registry.Register<MeshRenderer>(
			"kt.MeshRenderer", 1,
			[](const MeshRenderer& input) -> SceneValue
			{
				return SceneValue::Object{
					{"meshId", std::to_string(input.meshId)}, {"materialId", std::to_string(input.materialId)}, {"visible", input.visible}};
			},
			[](const SceneValue& data, const SceneReadContext&) -> MeshRenderer
			{
				data.RequireFields({"meshId", "materialId", "visible"});
				return {data.At("meshId").AsUint64(), data.At("materialId").AsUint64(), data.At("visible").AsBool()};
			});

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
		registry.Register(std::move(script));
	}
}
