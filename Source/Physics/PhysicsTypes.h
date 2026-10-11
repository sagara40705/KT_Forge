#pragma once
#include <Core/Math/Quaternion.h>
#include <array>
#include <cstdint>
#include <optional>
#include <numbers>

namespace KT::Physics
{
	using KT::Core::Math::Quaternion;
	using KT::Core::Math::Vector3;
	inline constexpr double FixedDeltaSeconds = 1.0 / 60.0;
	inline constexpr std::size_t LayerCount = 8;
	// 初速度の検証とJoltの速度制限で同じ上限を使う。単位はm/s・rad/s。
	inline constexpr float MaxLinearVelocity = 500.0f;
	inline constexpr float MaxAngularVelocity = 0.25f * std::numbers::pi_v<float> * 60.0f;

	// ゲーム側の位置・回転。長さはm、角度はrad、時間はsで扱う。
	struct Pose
	{
		Vector3 position;
		Quaternion rotation;
		bool operator==(const Pose&) const = default;
	};

	enum class MotionType
	{
		Static,
		Dynamic,
		Kinematic
	};
	enum class ShapeType
	{
		Box,
		Sphere,
		Capsule
	};

	// 初期設定だけを保持する。計算後の速度やsleep状態とは分離する。
	struct BodySettings
	{
		MotionType motion = MotionType::Static;
		float mass = 1.0f;
		float friction = 0.5f;
		float restitution = 0.0f;
		float linearDamping = 0.05f;
		float angularDamping = 0.05f;
		float gravityFactor = 1.0f;
		Vector3 linearVelocity;
		Vector3 angularVelocity;
		bool allowSleeping = true;
		bool continuous = false;
		bool operator==(const BodySettings&) const = default;
	};

	// Boxは半寸法、Capsuleは円柱部分の半高と半径。Y軸方向に伸びる。
	struct ColliderSettings
	{
		ShapeType shape = ShapeType::Box;
		Vector3 halfExtent{0.5f, 0.5f, 0.5f};
		float radius = 0.5f;
		float halfHeight = 0.5f;
		Pose offset;
		std::uint8_t layer = 0;
		bool sensor = false;
		bool operator==(const ColliderSettings&) const = default;
	};

	// 相互に対称な8層の衝突表。Sceneごとに所有し、準備後は変更しない。
	struct SceneSettings
	{
		Vector3 gravity{0.0f, -9.81f, 0.0f};
		std::array<std::uint8_t, LayerCount> collisionMasks{255, 255, 255, 255, 255, 255, 255, 255};
		bool operator==(const SceneSettings&) const = default;
	};

	// PhysicsWorld内だけで有効な論理ID。EntityやJolt BodyIDとは交換しない。
	struct BodyHandle
	{
		std::uint64_t value = 0;
		auto operator<=>(const BodyHandle&) const = default;
	};

	// 最新の計算結果。enabled=falseのBodyは衝突・ray対象から外れる。
	struct BodyState
	{
		Pose pose;
		Vector3 linearVelocity;
		Vector3 angularVelocity;
		bool sleeping = true;
		bool enabled = true;
	};

	enum class CommandType
	{
		Force,
		Impulse,
		Velocity,
		KinematicTarget,
		Teleport
	};

	// 1tickだけ有効な命令。TeleportのclearVelocity=falseは現在速度を保持する。
	struct BodyCommand
	{
		CommandType type = CommandType::Force;
		Vector3 linear;
		Vector3 angular;
		Pose pose;
		bool clearVelocity = false;
	};

	enum class ContactType
	{
		Added,
		Persisted,
		Removed
	};

	// Removedはsleep移行でも届く。幾何学的な離脱の判定とは区別する。
	struct ContactEvent
	{
		ContactType type = ContactType::Added;
		BodyHandle first;
		BodyHandle second;
		Vector3 normal;
		bool sensor = false;
	};

	// directionは単位ベクトルである必要はない。長さmaxDistanceのrayへ正規化する。
	struct Ray
	{
		Vector3 origin;
		Vector3 direction{0, 0, 1};
		float maxDistance = 1000.0f;
		std::uint8_t layerMask = 255;
		bool includeSensors = false;
		std::optional<BodyHandle> ignore;
	};

	struct RayHit
	{
		BodyHandle body;
		Vector3 position;
		Vector3 normal;
		float distance = 0;
	};

	void Validate(const Pose& pose);
	void Validate(const BodySettings& settings);
	void Validate(const ColliderSettings& settings);
	void Validate(const SceneSettings& settings);
	void Validate(const BodyCommand& command);
}
