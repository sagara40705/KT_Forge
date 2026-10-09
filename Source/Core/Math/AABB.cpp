#include <Core/Math/AABB.h>
#include <Core/Math/Scalar.h>
#include <algorithm>
#include <stdexcept>

namespace KT::Core::Math
{
	bool IsEmpty(const AABB& b) noexcept
	{
		return b.minimum == Vector3{Infinity, Infinity, Infinity} && b.maximum == Vector3{-Infinity, -Infinity, -Infinity};
	}

	bool IsValid(const AABB& b) noexcept
	{
		return IsEmpty(b) ||
			(IsFinite(b.minimum) && IsFinite(b.maximum) && b.minimum.x <= b.maximum.x && b.minimum.y <= b.maximum.y &&
				b.minimum.z <= b.maximum.z);
	}

	AABB AABBFromMinMax(Vector3 minimum, Vector3 maximum)
	{
		const AABB result{minimum, maximum};
		if (!IsValid(result) || IsEmpty(result))
		{
			throw std::invalid_argument("AABB requires finite ordered endpoints.");
		}
		return result;
	}

	bool Contains(const AABB& b, Vector3 point)
	{
		if (!IsValid(b) || !IsFinite(point))
		{
			throw std::invalid_argument("AABB Contains input is invalid.");
		}
		return !IsEmpty(b) && point.x >= b.minimum.x && point.x <= b.maximum.x && point.y >= b.minimum.y && point.y <= b.maximum.y &&
			point.z >= b.minimum.z && point.z <= b.maximum.z;
	}

	bool Intersects(const AABB& a, const AABB& b)
	{
		if (!IsValid(a) || !IsValid(b))
		{
			throw std::invalid_argument("AABB Intersects input is invalid.");
		}
		return !IsEmpty(a) && !IsEmpty(b) && a.minimum.x <= b.maximum.x && b.minimum.x <= a.maximum.x && a.minimum.y <= b.maximum.y &&
			b.minimum.y <= a.maximum.y && a.minimum.z <= b.maximum.z && b.minimum.z <= a.maximum.z;
	}

	AABB Expanded(const AABB& b, Vector3 point)
	{
		if (!IsValid(b) || !IsFinite(point))
		{
			throw std::invalid_argument("AABB Expanded input is invalid.");
		}
		if (IsEmpty(b))
		{
			return {point, point};
		}
		return {{std::min(b.minimum.x, point.x), std::min(b.minimum.y, point.y), std::min(b.minimum.z, point.z)},
			{std::max(b.maximum.x, point.x), std::max(b.maximum.y, point.y), std::max(b.maximum.z, point.z)}};
	}

	AABB Merged(const AABB& a, const AABB& b)
	{
		if (!IsValid(a) || !IsValid(b))
		{
			throw std::invalid_argument("AABB Merged input is invalid.");
		}
		if (IsEmpty(a))
		{
			return b;
		}
		if (IsEmpty(b))
		{
			return a;
		}
		return Expanded(Expanded(a, b.minimum), b.maximum);
	}

	Vector3 Center(const AABB& b)
	{
		if (!IsValid(b) || IsEmpty(b))
		{
			throw std::invalid_argument("AABB Center requires a finite nonempty box.");
		}
		return {Detail::CheckedFloat((double(b.minimum.x) + b.maximum.x) * 0.5),
			Detail::CheckedFloat((double(b.minimum.y) + b.maximum.y) * 0.5),
			Detail::CheckedFloat((double(b.minimum.z) + b.maximum.z) * 0.5)};
	}

	Vector3 Extents(const AABB& b)
	{
		if (!IsValid(b) || IsEmpty(b))
		{
			throw std::invalid_argument("AABB Extents requires a finite nonempty box.");
		}
		return {Detail::CheckedFloat((double(b.maximum.x) - b.minimum.x) * 0.5),
			Detail::CheckedFloat((double(b.maximum.y) - b.minimum.y) * 0.5),
			Detail::CheckedFloat((double(b.maximum.z) - b.minimum.z) * 0.5)};
	}
}
