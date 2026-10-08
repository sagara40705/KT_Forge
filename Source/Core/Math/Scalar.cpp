#include <Core/Math/Scalar.h>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace KT::Core::Math
{
	bool Near(float a, float b, float absolute, float relative) noexcept
	{
		if (!IsFinite(a) || !IsFinite(b) || !IsFinite(absolute) || !IsFinite(relative) || absolute < 0 || relative < 0)
			return false;
		const double tolerance = std::max(double(absolute), double(relative) * std::max(std::abs(double(a)), std::abs(double(b))));
		return std::abs(double(a) - b) <= tolerance;
	}
	float Clamp(float value, float minimum, float maximum)
	{
		if (!IsFinite(value) || !IsFinite(minimum) || !IsFinite(maximum) || minimum > maximum)
			throw std::invalid_argument("Clamp input is nonfinite or unordered.");
		return std::clamp(value, minimum, maximum);
	}
	float Detail::CheckedFloat(double value)
	{
		constexpr double limit = (std::numeric_limits<float>::max)();
		if (!std::isfinite(value) || value < -limit || value > limit)
			throw std::overflow_error("Math result cannot be represented as float.");
		return static_cast<float>(value);
	}
}
