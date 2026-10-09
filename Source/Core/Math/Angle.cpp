#include <Core/Math/Angle.h>
#include <Core/Math/Scalar.h>
#include <numbers>
#include <stdexcept>

namespace KT::Core::Math
{
	float DegreesToRadians(float degrees)
	{
		if (!IsFinite(degrees))
		{
			throw std::invalid_argument("Degree is nonfinite.");
		}
		return Detail::CheckedFloat(double(degrees) * std::numbers::pi / 180.0);
	}

	float RadiansToDegrees(float radians)
	{
		if (!IsFinite(radians))
		{
			throw std::invalid_argument("Radian is nonfinite.");
		}
		return Detail::CheckedFloat(double(radians) * 180.0 / std::numbers::pi);
	}
}
