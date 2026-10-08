#pragma once

namespace KT::Core::Math
{
	struct Degrees { float value = 0; explicit constexpr Degrees(float angle = 0) noexcept : value(angle) {} };
	struct Radians { float value = 0; explicit constexpr Radians(float angle = 0) noexcept : value(angle) {} };
	// 既存float APIはradian。新しい呼出では型で角度単位を明示できる。
	float DegreesToRadians(float degrees);
	float RadiansToDegrees(float radians);
	inline Radians ToRadians(Degrees angle) { return Radians(DegreesToRadians(angle.value)); }
	inline Degrees ToDegrees(Radians angle) { return Degrees(RadiansToDegrees(angle.value)); }
}
