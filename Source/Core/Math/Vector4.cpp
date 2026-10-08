#include <Core/Math/Vector4.h>
#include <Core/Math/Scalar.h>
#include <cmath>
#include <stdexcept>

namespace KT::Core::Math
{
	bool IsFinite(Vector4 v) noexcept { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::isfinite(v.w); }
	float Dot(Vector4 a, Vector4 b)
	{
		if (!IsFinite(a) || !IsFinite(b)) throw std::invalid_argument("Dot input must be finite.");
		return Detail::CheckedFloat(double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z + double(a.w) * b.w);
	}

	float Length(Vector4 v)
	{
		if (!IsFinite(v)) throw std::invalid_argument("Length input must be finite.");
		return Detail::CheckedFloat(std::sqrt(double(v.x) * v.x + double(v.y) * v.y + double(v.z) * v.z + double(v.w) * v.w));
	}
	Vector4 Normalized(Vector4 v)
	{
		if (!IsFinite(v)) throw std::invalid_argument("Normalized input must be finite.");
		const double length = std::sqrt(double(v.x) * v.x + double(v.y) * v.y + double(v.z) * v.z + double(v.w) * v.w);
		if (length == 0) throw std::invalid_argument("Normalized input must be nonzero.");
		return {float(v.x / length), float(v.y / length), float(v.z / length), float(v.w / length)};
	}
	Vector4 Lerp(Vector4 a, Vector4 b, float t)
	{
		if (!IsFinite(a) || !IsFinite(b) || !IsFinite(t)) throw std::invalid_argument("Lerp input must be finite.");
		return {Detail::CheckedFloat(double(a.x) + (double(b.x) - a.x) * t), Detail::CheckedFloat(double(a.y) + (double(b.y) - a.y) * t), Detail::CheckedFloat(double(a.z) + (double(b.z) - a.z) * t), Detail::CheckedFloat(double(a.w) + (double(b.w) - a.w) * t)};
	}
}
