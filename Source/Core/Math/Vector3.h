#pragma once

namespace KT::Core::Math
{
	struct Vector3
	{
		float x = 0, y = 0, z = 0;
		constexpr bool operator==(const Vector3&) const noexcept = default;
	};
	// 基本演算子は通常のfloat演算。zero除算/overflowを検査しない。
	constexpr Vector3 operator+(Vector3 a, Vector3 b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
	constexpr Vector3 operator-(Vector3 a, Vector3 b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
	constexpr Vector3 operator-(Vector3 v) noexcept { return {-v.x, -v.y, -v.z}; }
	constexpr Vector3 operator*(Vector3 v, float scalar) noexcept { return {v.x * scalar, v.y * scalar, v.z * scalar}; }
	constexpr Vector3 operator*(float scalar, Vector3 v) noexcept { return v * scalar; }
	constexpr Vector3 operator/(Vector3 v, float scalar) noexcept { return {v.x / scalar, v.y / scalar, v.z / scalar}; }
	bool IsFinite(Vector3 value) noexcept;
	// 以下の関数はfinite入力を検査し、float結果の範囲外はoverflow_errorで拒否する。
	float Dot(Vector3 first, Vector3 second);
	Vector3 Cross(Vector3 first, Vector3 second); // Cross(+X,+Y)=+Z。
	float Length(Vector3 value);
	Vector3 Normalized(Vector3 value); // finite/nonzero。Quaternionの既存Normalizeと区別する。
	Vector3 Lerp(Vector3 first, Vector3 second, float t); // finiteなtで外挿も許可。
}
