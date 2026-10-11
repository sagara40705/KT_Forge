#include <World/Physics/PhysicsSerialization.h>
#include <World/Scene/ComponentRegistry.h>
#include <cmath>

namespace KT::World
{
	using namespace KT::Physics;

	static SceneValue VectorValue(Vector3 value)
	{
		return SceneValue::Array{double(value.x), double(value.y), double(value.z)};
	}

	static Vector3 ReadVector(const SceneValue& data)
	{
		const auto& array = data.AsArray();
		if (array.size() != 3)
		{
			throw std::invalid_argument("物理のvector設定には3要素が必要です。");
		}
		return {array[0].AsFloat(), array[1].AsFloat(), array[2].AsFloat()};
	}

	static std::uint8_t ReadByte(const SceneValue& data)
	{
		const double number = data.AsNumber();
		if (!std::isfinite(number) || number < 0 || number > 255 || std::floor(number) != number)
		{
			throw std::invalid_argument("物理のlayer・maskには0..255の整数が必要です。");
		}
		return static_cast<std::uint8_t>(number);
	}

	SceneValue EncodePhysics(const BodySettings& input)
	{
		Validate(input);
		const auto* motion = input.motion == MotionType::Static ? "static" : input.motion == MotionType::Dynamic ? "dynamic" : "kinematic";
		return SceneValue::Object{{"motion", motion}, {"mass", double(input.mass)}, {"friction", double(input.friction)},
			{"restitution", double(input.restitution)}, {"linearDamping", double(input.linearDamping)},
			{"angularDamping", double(input.angularDamping)}, {"gravityFactor", double(input.gravityFactor)},
			{"linearVelocity", VectorValue(input.linearVelocity)}, {"angularVelocity", VectorValue(input.angularVelocity)},
			{"allowSleeping", input.allowSleeping}, {"continuous", input.continuous}};
	}

	BodySettings DecodeBodySettings(const SceneValue& data)
	{
		data.RequireFields({"motion", "mass", "friction", "restitution", "linearDamping", "angularDamping", "gravityFactor",
			"linearVelocity", "angularVelocity", "allowSleeping", "continuous"});
		BodySettings result;
		const auto& motion = data.At("motion").AsString();
		if (motion == "static")
		{
			result.motion = MotionType::Static;
		}
		else if (motion == "dynamic")
		{
			result.motion = MotionType::Dynamic;
		}
		else if (motion == "kinematic")
		{
			result.motion = MotionType::Kinematic;
		}
		else
		{
			throw std::invalid_argument("RigidBodyのmotion文字列が未対応です。");
		}
		result.mass = data.At("mass").AsFloat();
		result.friction = data.At("friction").AsFloat();
		result.restitution = data.At("restitution").AsFloat();
		result.linearDamping = data.At("linearDamping").AsFloat();
		result.angularDamping = data.At("angularDamping").AsFloat();
		result.gravityFactor = data.At("gravityFactor").AsFloat();
		result.linearVelocity = ReadVector(data.At("linearVelocity"));
		result.angularVelocity = ReadVector(data.At("angularVelocity"));
		result.allowSleeping = data.At("allowSleeping").AsBool();
		result.continuous = data.At("continuous").AsBool();
		Validate(result);
		return result;
	}

	SceneValue EncodePhysics(const ColliderSettings& input)
	{
		Validate(input);
		const auto* shape = input.shape == ShapeType::Box ? "box" : input.shape == ShapeType::Sphere ? "sphere" : "capsule";
		const auto rotation = input.offset.rotation;
		return SceneValue::Object{{"shape", shape}, {"halfExtent", VectorValue(input.halfExtent)}, {"radius", double(input.radius)},
			{"halfHeight", double(input.halfHeight)}, {"center", VectorValue(input.offset.position)},
			{"rotation", SceneValue::Array{double(rotation.x), double(rotation.y), double(rotation.z), double(rotation.w)}},
			{"layer", double(input.layer)}, {"sensor", input.sensor}};
	}

	ColliderSettings DecodeColliderSettings(const SceneValue& data)
	{
		data.RequireFields({"shape", "halfExtent", "radius", "halfHeight", "center", "rotation", "layer", "sensor"});
		ColliderSettings result;
		const auto& shape = data.At("shape").AsString();
		if (shape == "box")
		{
			result.shape = ShapeType::Box;
		}
		else if (shape == "sphere")
		{
			result.shape = ShapeType::Sphere;
		}
		else if (shape == "capsule")
		{
			result.shape = ShapeType::Capsule;
		}
		else
		{
			throw std::invalid_argument("Colliderのshape文字列が未対応です。");
		}
		result.halfExtent = ReadVector(data.At("halfExtent"));
		result.radius = data.At("radius").AsFloat();
		result.halfHeight = data.At("halfHeight").AsFloat();
		result.offset.position = ReadVector(data.At("center"));
		const auto& rotation = data.At("rotation").AsArray();
		if (rotation.size() != 4)
		{
			throw std::invalid_argument("Colliderの局所回転はxyzwの4要素が必要です。");
		}
		result.offset.rotation = {rotation[0].AsFloat(), rotation[1].AsFloat(), rotation[2].AsFloat(), rotation[3].AsFloat()};
		result.layer = ReadByte(data.At("layer"));
		result.sensor = data.At("sensor").AsBool();
		Validate(result);
		return result;
	}

	SceneValue EncodePhysics(const SceneSettings& input)
	{
		Validate(input);
		SceneValue::Array masks;
		for (const auto mask : input.collisionMasks)
		{
			masks.emplace_back(double(mask));
		}
		return SceneValue::Object{{"gravity", VectorValue(input.gravity)}, {"collisionMasks", std::move(masks)}};
	}

	SceneSettings DecodePhysicsSettings(const SceneValue& data)
	{
		data.RequireFields({"gravity", "collisionMasks"});
		SceneSettings result;
		result.gravity = ReadVector(data.At("gravity"));
		const auto& masks = data.At("collisionMasks").AsArray();
		if (masks.size() != LayerCount)
		{
			throw std::invalid_argument("Sceneの衝突layer表には8行が必要です。");
		}
		for (std::size_t index = 0; index < masks.size(); ++index)
		{
			result.collisionMasks[index] = ReadByte(masks[index]);
		}
		Validate(result);
		return result;
	}

	void RegisterPhysicsComponents(ComponentRegistry& registry)
	{
		registry.Register<RigidBody>(
			"kt.RigidBody", 1, [](const RigidBody& input) { return EncodePhysics(input.Settings()); },
			[](const SceneValue& data, const SceneReadContext&) { return RigidBody(DecodeBodySettings(data)); });
		registry.Register<Collider>(
			"kt.Collider", 1, [](const Collider& input) { return EncodePhysics(input.Settings()); },
			[](const SceneValue& data, const SceneReadContext&) { return Collider(DecodeColliderSettings(data)); });
	}

	void ValidatePhysicsObjects(const SceneAsset& asset)
	{
		for (const auto& object : asset.objects)
		{
			bool rigidBody = false;
			bool collider = false;
			const SceneValue* transform = nullptr;
			for (const auto& component : object.components)
			{
				rigidBody |= component.type == "kt.RigidBody";
				collider |= component.type == "kt.Collider";
				if (component.type == "kt.LocalTransform")
				{
					transform = &component.data;
				}
			}
			if (rigidBody || collider)
			{
				if (!rigidBody || !collider || !transform || object.parent)
				{
					throw std::invalid_argument(
						"物理EntityにはRigidBody・Collider・LocalTransformと親なしの設定が必要です: " + object.uuid.ToString());
				}
				if (ReadVector(transform->At("scale")) != Vector3{1, 1, 1})
				{
					throw std::invalid_argument("物理Entityのscaleは(1,1,1)にしてください: " + object.uuid.ToString());
				}
			}
		}
	}
}
