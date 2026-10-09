#include <Core/Math/Plane.h>
#include <Core/Math/Scalar.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace KT::Core::Math
{
	bool IsValid(const Plane& p) noexcept
	{
		return IsFinite(p.normal) && IsFinite(p.distance) &&
			std::abs(std::hypot(double(p.normal.x), double(p.normal.y), double(p.normal.z)) - 1.0) <= 1e-5;
	}

	Plane PlaneFromCoefficients(double x, double y, double z, double distance)
	{
		if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(distance))
		{
			throw std::invalid_argument("Plane coefficients must be finite.");
		}
		// 最大成分で縮小してから長さを測る。double最大値付近の係数でも長さoverflowを避ける。
		const double scale = std::max(std::abs(x), std::max(std::abs(y), std::abs(z)));
		if (scale == 0)
		{
			throw std::invalid_argument("Plane normal must be nonzero.");
		}
		const double length = std::hypot(x / scale, y / scale, z / scale);
		const Plane result{{Detail::CheckedFloat((x / scale) / length), Detail::CheckedFloat((y / scale) / length),
							   Detail::CheckedFloat((z / scale) / length)},
			Detail::CheckedFloat((distance / scale) / length)};
		if (!IsValid(result))
		{
			throw std::overflow_error("Normalized plane cannot be represented.");
		}
		return result;
	}

	Plane PlaneFromPointNormal(Vector3 point, Vector3 normal)
	{
		if (!IsFinite(point))
		{
			throw std::invalid_argument("Plane point must be finite.");
		}
		const auto n = Normalized(normal);
		return PlaneFromCoefficients(n.x, n.y, n.z, -(double(n.x) * point.x + double(n.y) * point.y + double(n.z) * point.z));
	}

	Plane Normalized(const Plane& p)
	{
		return PlaneFromCoefficients(p.normal.x, p.normal.y, p.normal.z, p.distance);
	}

	float SignedDistance(const Plane& p, Vector3 point)
	{
		if (!IsValid(p) || !IsFinite(point))
		{
			throw std::invalid_argument("SignedDistance input is invalid.");
		}
		return Detail::CheckedFloat(
			double(p.normal.x) * point.x + double(p.normal.y) * point.y + double(p.normal.z) * point.z + p.distance);
	}

	Vector3 ProjectOntoPlane(Vector3 point, const Plane& p)
	{
		if (!IsValid(p) || !IsFinite(point))
		{
			throw std::invalid_argument("Plane projection input is invalid.");
		}
		const double squared = double(p.normal.x) * p.normal.x + double(p.normal.y) * p.normal.y + double(p.normal.z) * p.normal.z;
		const double offset =
			(double(p.normal.x) * point.x + double(p.normal.y) * point.y + double(p.normal.z) * point.z + p.distance) / squared;
		return {Detail::CheckedFloat(point.x - p.normal.x * offset), Detail::CheckedFloat(point.y - p.normal.y * offset),
			Detail::CheckedFloat(point.z - p.normal.z * offset)};
	}
}
