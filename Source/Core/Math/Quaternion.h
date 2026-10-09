#pragma once
#include <Core/Math/Vector3.h>
#include <Core/Math/Angle.h>

namespace KT::Core::Math
{
	struct Quaternion
	{
		float x = 0, y = 0, z = 0, w = 1; // xyzw。既定値はidentity回転。
		constexpr bool operator==(const Quaternion&) const noexcept = default;
	};

	bool IsFinite(Quaternion value) noexcept;
	Quaternion Normalize(Quaternion value); // finite/nonzeroが必要。入力は変更しない。
	Quaternion FromAxisAngle(Vector3 axis, float radians);

	inline Quaternion FromAxisAngle(Vector3 axis, Radians angle)
	{
		return FromAxisAngle(axis, angle.value);
	}

	// first→secondの適用順。単位QuaternionではToMatrix(first*second)=ToMatrix(first)*ToMatrix(second)。
	// 積は一般のQuaternion積で、zeroも許可し勝手に正規化しない。回転利用時だけNormalizeする。
	Quaternion Multiply(Quaternion first, Quaternion second);

	inline Quaternion operator*(Quaternion first, Quaternion second)
	{
		return Multiply(first, second);
	}

	constexpr Quaternion operator-(Quaternion q) noexcept
	{
		return {-q.x, -q.y, -q.z, -q.w};
	}

	constexpr Quaternion Conjugate(Quaternion q) noexcept
	{
		return {-q.x, -q.y, -q.z, q.w};
	}

	Vector3 Rotate(Vector3 value, Quaternion rotation);				// 回転を正規化して適用。zero/nonfiniteを拒否。
	Quaternion Slerp(Quaternion first, Quaternion second, float t); // t=[0,1]、最短弧、両入力を正規化。
}
