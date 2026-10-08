#pragma once

namespace KT::Core::Math
{
	struct Vector4
	{
		float x = 0, y = 0, z = 0, w = 0;
		constexpr bool operator==(const Vector4&) const noexcept = default;
	};
	// 基本演算子は通常のfloat演算。zero除算/overflowを検査しない。
	constexpr Vector4 operator+(Vector4 a, Vector4 b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
	constexpr Vector4 operator-(Vector4 a, Vector4 b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }
	constexpr Vector4 operator-(Vector4 v) noexcept { return {-v.x, -v.y, -v.z, -v.w}; }
	constexpr Vector4 operator*(Vector4 v, float scalar) noexcept { return {v.x * scalar, v.y * scalar, v.z * scalar, v.w * scalar}; }
	constexpr Vector4 operator*(float scalar, Vector4 v) noexcept { return v * scalar; }
	constexpr Vector4 operator/(Vector4 v, float scalar) noexcept { return {v.x / scalar, v.y / scalar, v.z / scalar, v.w / scalar}; }
	bool IsFinite(Vector4 value) noexcept;
	// 以下の関数はfinite入力を検査し、float結果の範囲外はoverflow_errorで拒否する。
	float Dot(Vector4 first, Vector4 second);
	float Length(Vector4 value);
	Vector4 Normalized(Vector4 value); // finite/nonzero。Quaternionの既存Normalizeと区別する。
	Vector4 Lerp(Vector4 first, Vector4 second, float t); // finiteなtで外挿も許可。
}
