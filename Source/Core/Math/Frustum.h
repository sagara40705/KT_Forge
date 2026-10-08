#pragma once
#include <Core/Math/AABB.h>
#include <Core/Math/Plane.h>
#include <Core/Math/Matrix4.h>
#include <array>

namespace KT::Core::Math
{
	enum class FrustumSide { Left, Right, Bottom, Top, Near, Far };
	struct Frustum
	{
		// 既定値はidentityのclip領域 -w<=x,y<=w、0<=z<=w。各normalは内向き。
		std::array<Plane,6> planes{{{{1,0,0},1},{{-1,0,0},1},{{0,1,0},1},
			{{0,-1,0},1},{{0,0,-1},1},{{0,0,1},0}}};
		const Plane& GetPlane(FrustumSide side) const;
	};
	bool IsValid(const Frustum& frustum) noexcept; // 全planeのvalidity。任意の6平面の体積/有界性は証明しない。
	// KTの有限far Reverse-Z view*projection専用。row-vectorなので列から抽出する。
	Frustum FrustumFromViewProjection(const Matrix4& viewProjection);
	bool Contains(const Frustum& frustum, Vector3 point, float tolerance=0);
	// 保守的な6平面判定。falseなら外側、trueには偽陽性があり得る（完全SATではない）。emptyはfalse。
	bool Intersects(const Frustum& frustum, const AABB& box, float tolerance=0);
}
