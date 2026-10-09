#include <Core/Math/Frustum.h>
#include <Core/Math/Scalar.h>
#include <algorithm>
#include <stdexcept>

namespace KT::Core::Math
{
	const Plane& Frustum::GetPlane(FrustumSide side) const
	{
		return planes.at(static_cast<std::size_t>(side));
	}

	bool IsValid(const Frustum& f) noexcept
	{
		return std::all_of(f.planes.begin(), f.planes.end(), [](const Plane& p) { return IsValid(p); });
	}

	Frustum FrustumFromViewProjection(const Matrix4& m)
	{
		(void)Inverse(m); // finite/特異/数値的不安定を既存の逆行列契約で拒否する。
		const auto combined = [&](std::size_t column, double sign)
		{
			return PlaneFromCoefficients(double(m(0, 3)) + sign * m(0, column), double(m(1, 3)) + sign * m(1, column),
				double(m(2, 3)) + sign * m(2, column), double(m(3, 3)) + sign * m(3, column));
		};
		Frustum result;
		result.planes = {combined(0, 1), combined(0, -1), combined(1, 1), combined(1, -1), combined(2, -1),
			PlaneFromCoefficients(m(0, 2), m(1, 2), m(2, 2), m(3, 2))};
		// Reverse-Z: Nearはcolumn3-column2（z<=w）、Farはcolumn2（z>=0）。
		// 列番号は0始まり。column3はclip.w、column2はclip.z。
		return result;
	}

	bool Contains(const Frustum& f, Vector3 point, float tolerance)
	{
		if (!IsValid(f) || !IsFinite(point) || !IsFinite(tolerance) || tolerance < 0)
		{
			throw std::invalid_argument("Frustum Contains input is invalid.");
		}
		for (const auto& p : f.planes)
		{
			if (double(p.normal.x) * point.x + double(p.normal.y) * point.y + double(p.normal.z) * point.z + p.distance <
				-double(tolerance))
			{
				return false;
			}
		}
		return true;
	}

	bool Intersects(const Frustum& f, const AABB& box, float tolerance)
	{
		if (!IsValid(f) || !IsValid(box) || !IsFinite(tolerance) || tolerance < 0)
		{
			throw std::invalid_argument("Frustum Intersects input is invalid.");
		}
		if (IsEmpty(box))
		{
			return false;
		}
		for (const auto& p : f.planes)
		{
			const Vector3 support{p.normal.x >= 0 ? box.maximum.x : box.minimum.x, p.normal.y >= 0 ? box.maximum.y : box.minimum.y,
				p.normal.z >= 0 ? box.maximum.z : box.minimum.z};
			if (double(p.normal.x) * support.x + double(p.normal.y) * support.y + double(p.normal.z) * support.z + p.distance <
				-double(tolerance))
			{
				return false;
			}
		}
		return true;
	}
}
