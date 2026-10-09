#include <Core/Math/Vector3.h>
#include <Core/Math/Scalar.h>
#include <cmath>
#include <stdexcept>

namespace KT::Core::Math
{
	bool IsFinite(Vector3 v) noexcept
	{
		return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
	}

	float Dot(Vector3 a, Vector3 b)
	{
		if (!IsFinite(a) || !IsFinite(b))
		{
			throw std::invalid_argument("Dot input must be finite.");
		}
		return Detail::CheckedFloat(double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z);
	}

	Vector3 Cross(Vector3 a, Vector3 b)
	{
		if (!IsFinite(a) || !IsFinite(b))
		{
			throw std::invalid_argument("Cross input must be finite.");
		}
		return {Detail::CheckedFloat(double(a.y) * b.z - double(a.z) * b.y), Detail::CheckedFloat(double(a.z) * b.x - double(a.x) * b.z),
			Detail::CheckedFloat(double(a.x) * b.y - double(a.y) * b.x)};
	}

	float Length(Vector3 v)
	{
		if (!IsFinite(v))
		{
			throw std::invalid_argument("Length input must be finite.");
		}
		return Detail::CheckedFloat(std::sqrt(double(v.x) * v.x + double(v.y) * v.y + double(v.z) * v.z));
	}

	Vector3 Normalized(Vector3 v)
	{
		if (!IsFinite(v))
		{
			throw std::invalid_argument("Normalized input must be finite.");
		}
		const double length = std::sqrt(double(v.x) * v.x + double(v.y) * v.y + double(v.z) * v.z);
		if (length == 0)
		{
			throw std::invalid_argument("Normalized input must be nonzero.");
		}
		return {float(v.x / length), float(v.y / length), float(v.z / length)};
	}

	Vector3 Lerp(Vector3 a, Vector3 b, float t)
	{
		if (!IsFinite(a) || !IsFinite(b) || !IsFinite(t))
		{
			throw std::invalid_argument("Lerp input must be finite.");
		}
		return {Detail::CheckedFloat(double(a.x) + (double(b.x) - a.x) * t), Detail::CheckedFloat(double(a.y) + (double(b.y) - a.y) * t),
			Detail::CheckedFloat(double(a.z) + (double(b.z) - a.z) * t)};
	}
}
