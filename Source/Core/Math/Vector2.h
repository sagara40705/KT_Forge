#pragma once

namespace KT::Core::Math
{
	struct Vector2
	{
		float x = 0, y = 0;
		constexpr bool operator==(const Vector2&) const noexcept = default;
	};

	// 基本演算子は通常のfloat演算。zero除算/overflowを検査しない。
	constexpr Vector2 operator+(Vector2 a, Vector2 b) noexcept
	{
		return {a.x + b.x, a.y + b.y};
	}

	constexpr Vector2 operator-(Vector2 a, Vector2 b) noexcept
	{
		return {a.x - b.x, a.y - b.y};
	}

	constexpr Vector2 operator-(Vector2 v) noexcept
	{
		return {-v.x, -v.y};
	}

	constexpr Vector2 operator*(Vector2 v, float scalar) noexcept
	{
		return {v.x * scalar, v.y * scalar};
	}

	constexpr Vector2 operator*(float scalar, Vector2 v) noexcept
	{
		return v * scalar;
	}

	constexpr Vector2 operator/(Vector2 v, float scalar) noexcept
	{
		return {v.x / scalar, v.y / scalar};
	}

	bool IsFinite(Vector2 value) noexcept;
	// 以下の関数はfinite入力を検査し、float結果の範囲外はoverflow_errorで拒否する。
	float Dot(Vector2 first, Vector2 second);
	float Length(Vector2 value);
	Vector2 Normalized(Vector2 value);					  // finite/nonzero。Quaternionの既存Normalizeと区別する。
	Vector2 Lerp(Vector2 first, Vector2 second, float t); // finiteなtで外挿も許可。
}
