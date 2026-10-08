#pragma once
#include <numbers>
#include <limits>

namespace KT::Core::Math
{
	inline constexpr float Pi = std::numbers::pi_v<float>;
	inline constexpr float TwoPi = 2 * Pi;
	inline constexpr float HalfPi = Pi / 2;
	inline constexpr float Infinity = std::numeric_limits<float>::infinity();
}
