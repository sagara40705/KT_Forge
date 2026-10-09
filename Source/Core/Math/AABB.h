#pragma once
#include <Core/Math/Vector3.h>
#include <Core/Math/Constants.h>

namespace KT::Core::Math
{
	struct AABB
	{
		// この6値の組だけをcanonical emptyとする。縮退した有限boxは空ではない。
		Vector3 minimum{Infinity, Infinity, Infinity};
		Vector3 maximum{-Infinity, -Infinity, -Infinity};
	};

	bool IsEmpty(const AABB& box) noexcept;
	bool IsValid(const AABB& box) noexcept; // canonical empty またはfiniteかつmin<=max。
	AABB AABBFromMinMax(Vector3 minimum, Vector3 maximum);
	bool Contains(const AABB& box, Vector3 point);			// 境界含む。emptyならfalse。
	bool Intersects(const AABB& first, const AABB& second); // 接触含む。emptyならfalse。
	AABB Expanded(const AABB& box, Vector3 point);
	AABB Merged(const AABB& first, const AABB& second);
	Vector3 Center(const AABB& box);  // emptyは拒否。
	Vector3 Extents(const AABB& box); // 半幅。emptyは拒否。
}
