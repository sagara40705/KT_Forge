#include <Core/Math/Ray.h>
#include <Core/Math/Scalar.h>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace KT::Core::Math
{
	bool IsValid(const Ray& r) noexcept
	{
		return IsFinite(r.origin) && IsFinite(r.direction) && r.direction != Vector3{};
	}

	Ray MakeRay(Vector3 origin, Vector3 direction)
	{
		if (!IsFinite(origin))
		{
			throw std::invalid_argument("Ray origin must be finite.");
		}
		return {origin, Normalized(direction)};
	}

	Vector3 PointAt(const Ray& r, float t)
	{
		if (!IsValid(r) || !IsFinite(t))
		{
			throw std::invalid_argument("Ray PointAt input is invalid.");
		}
		return {Detail::CheckedFloat(r.origin.x + double(r.direction.x) * t), Detail::CheckedFloat(r.origin.y + double(r.direction.y) * t),
			Detail::CheckedFloat(r.origin.z + double(r.direction.z) * t)};
	}

	std::optional<RayInterval> Intersect(const Ray& r, const AABB& b)
	{
		if (!IsValid(r) || !IsValid(b))
		{
			throw std::invalid_argument("Ray/AABB input is invalid.");
		}
		if (IsEmpty(b))
		{
			return std::nullopt;
		}
		double near = 0, far = std::numeric_limits<double>::infinity();
		const auto slab = [&](float origin, float direction, float minimum, float maximum)
		{
			if (direction == 0)
			{
				return origin >= minimum && origin <= maximum;
			}
			double first = (double(minimum) - origin) / direction, second = (double(maximum) - origin) / direction;
			if (first > second)
			{
				std::swap(first, second);
			}
			near = std::max(near, first);
			far = std::min(far, second);
			return near <= far;
		};
		if (!slab(r.origin.x, r.direction.x, b.minimum.x, b.maximum.x) || !slab(r.origin.y, r.direction.y, b.minimum.y, b.maximum.y) ||
			!slab(r.origin.z, r.direction.z, b.minimum.z, b.maximum.z))
		{
			return std::nullopt;
		}
		return RayInterval{Detail::CheckedFloat(near), Detail::CheckedFloat(far)};
	}

	std::optional<float> Intersect(const Ray& r, const Plane& p)
	{
		if (!IsValid(r) || !IsValid(p))
		{
			throw std::invalid_argument("Ray/Plane input is invalid.");
		}
		const double divisor = double(p.normal.x) * r.direction.x + double(p.normal.y) * r.direction.y + double(p.normal.z) * r.direction.z;
		if (divisor == 0)
		{
			return std::nullopt;
		}
		const double t =
			-(double(p.normal.x) * r.origin.x + double(p.normal.y) * r.origin.y + double(p.normal.z) * r.origin.z + p.distance) / divisor;
		if (t < 0)
		{
			return std::nullopt;
		}
		return Detail::CheckedFloat(t);
	}
}
