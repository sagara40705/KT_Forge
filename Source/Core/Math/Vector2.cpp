#include <Core/Math/Vector2.h>
#include <Core/Math/Scalar.h>
#include <cmath>
#include <stdexcept>

namespace KT::Core::Math
{
	bool IsFinite(Vector2 v) noexcept { return std::isfinite(v.x) && std::isfinite(v.y); }
	float Dot(Vector2 a, Vector2 b)
	{
		if (!IsFinite(a) || !IsFinite(b)) throw std::invalid_argument("Dot input must be finite.");
		return Detail::CheckedFloat(double(a.x) * b.x + double(a.y) * b.y);
	}

	float Length(Vector2 v)
	{
		if (!IsFinite(v)) throw std::invalid_argument("Length input must be finite.");
		return Detail::CheckedFloat(std::sqrt(double(v.x) * v.x + double(v.y) * v.y));
	}
	Vector2 Normalized(Vector2 v)
	{
		if (!IsFinite(v)) throw std::invalid_argument("Normalized input must be finite.");
		const double length = std::sqrt(double(v.x) * v.x + double(v.y) * v.y);
		if (length == 0) throw std::invalid_argument("Normalized input must be nonzero.");
		return {float(v.x / length), float(v.y / length)};
	}
	Vector2 Lerp(Vector2 a, Vector2 b, float t)
	{
		if (!IsFinite(a) || !IsFinite(b) || !IsFinite(t)) throw std::invalid_argument("Lerp input must be finite.");
		return {Detail::CheckedFloat(double(a.x) + (double(b.x) - a.x) * t), Detail::CheckedFloat(double(a.y) + (double(b.y) - a.y) * t)};
	}
}
