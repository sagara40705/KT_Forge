#include <Physics/PhysicsTypes.h>
#include <cmath>
#include <stdexcept>

namespace KT::Physics
{
	void Validate(const Pose& pose)
	{
		if (!KT::Core::Math::IsFinite(pose.position) || std::abs(pose.position.x) > 1.0e6f || std::abs(pose.position.y) > 1.0e6f ||
			std::abs(pose.position.z) > 1.0e6f)
		{
			throw std::invalid_argument("単一精度の物理位置は各軸-1e6..1e6の有限値が必要です。");
		}
		(void)KT::Core::Math::Normalize(pose.rotation);
	}

	void Validate(const BodySettings& settings)
	{
		if (settings.motion != MotionType::Static && settings.motion != MotionType::Dynamic && settings.motion != MotionType::Kinematic)
		{
			throw std::invalid_argument("RigidBodyの運動種別が未対応です。");
		}
		if (!std::isfinite(settings.mass) || settings.mass < 1.0e-6f || settings.mass > 1.0e8f)
		{
			throw std::invalid_argument("RigidBodyの質量は1e-6..1e8が必要です。");
		}
		if (!std::isfinite(settings.friction) || settings.friction < 0 || settings.friction > 1 || !std::isfinite(settings.restitution) ||
			settings.restitution < 0 || settings.restitution > 1)
		{
			throw std::invalid_argument("RigidBodyの摩擦・反発係数は0..1が必要です。");
		}
		if (!std::isfinite(settings.linearDamping) || settings.linearDamping < 0 || settings.linearDamping > 1 ||
			!std::isfinite(settings.angularDamping) || settings.angularDamping < 0 || settings.angularDamping > 1)
		{
			throw std::invalid_argument("RigidBodyの減衰は0..1が必要です。");
		}
		if (!std::isfinite(settings.gravityFactor) || std::abs(settings.gravityFactor) > 1000)
		{
			throw std::invalid_argument("RigidBodyの重力倍率は-1000..1000の有限値が必要です。");
		}

		// 成分ごとの検査では斜め方向の上限超過を見逃すため、大きさをdoubleで調べる。
		const auto linear = settings.linearVelocity;
		if (!KT::Core::Math::IsFinite(linear) || std::hypot(double(linear.x), double(linear.y), double(linear.z)) > MaxLinearVelocity)
		{
			throw std::invalid_argument("RigidBodyの初期線速度は有限かつ大きさ500m/s以下が必要です。");
		}
		const auto angular = settings.angularVelocity;
		if (!KT::Core::Math::IsFinite(angular) ||
			std::hypot(double(angular.x), double(angular.y), double(angular.z)) > MaxAngularVelocity)
		{
			throw std::invalid_argument("RigidBodyの初期角速度は有限かつ大きさ15πrad/s以下が必要です。");
		}
	}

	void Validate(const ColliderSettings& settings)
	{
		Validate(settings.offset);
		if (settings.layer >= LayerCount)
		{
			throw std::invalid_argument("Colliderのlayerは0..7が必要です。");
		}
		if (settings.shape != ShapeType::Box && settings.shape != ShapeType::Sphere && settings.shape != ShapeType::Capsule)
		{
			throw std::invalid_argument("Colliderの形状が未対応です。");
		}
		const auto extent = settings.halfExtent;
		if (!KT::Core::Math::IsFinite(extent) || extent.x < 1.0e-4f || extent.y < 1.0e-4f || extent.z < 1.0e-4f || extent.x > 1.0e5f ||
			extent.y > 1.0e5f || extent.z > 1.0e5f)
		{
			throw std::invalid_argument("ColliderのBox半寸法は1e-4..1e5が必要です。");
		}
		if (!std::isfinite(settings.radius) || settings.radius < 1.0e-4f || settings.radius > 1.0e5f ||
			!std::isfinite(settings.halfHeight) || settings.halfHeight < 1.0e-4f || settings.halfHeight > 1.0e5f)
		{
			throw std::invalid_argument("Colliderの半径・円柱半高は1e-4..1e5が必要です。");
		}
	}

	void Validate(const SceneSettings& settings)
	{
		if (!KT::Core::Math::IsFinite(settings.gravity))
		{
			throw std::invalid_argument("Sceneの重力には有限値が必要です。");
		}
		for (std::size_t first = 0; first < LayerCount; ++first)
		{
			for (std::size_t second = 0; second < LayerCount; ++second)
			{
				if (((settings.collisionMasks[first] >> second) & 1) != ((settings.collisionMasks[second] >> first) & 1))
				{
					throw std::invalid_argument("Sceneの衝突layer表は対称である必要があります。");
				}
			}
		}
	}

	void Validate(const BodyCommand& command)
	{
		if (command.type < CommandType::Force || command.type > CommandType::Teleport)
		{
			throw std::invalid_argument("物理命令の種別が未対応です。");
		}
		if (!KT::Core::Math::IsFinite(command.linear) || !KT::Core::Math::IsFinite(command.angular))
		{
			throw std::invalid_argument("物理命令の力・速度には有限値が必要です。");
		}
		Validate(command.pose);
	}
}
