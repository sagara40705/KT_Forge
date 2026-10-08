#pragma once
#include <Core/Math/AABB.h>
#include <Core/Math/Plane.h>
#include <optional>

namespace KT::Core::Math
{
	struct Ray
	{
		Vector3 origin{};
		Vector3 direction{0,0,1}; // finite/nonzero。直接構築時は単位長とは限らない。
	};
	struct RayInterval { float near=0, far=0; };
	bool IsValid(const Ray& ray) noexcept;
	Ray MakeRay(Vector3 origin, Vector3 direction); // directionを正規化。tを距離として利用できる。
	Vector3 PointAt(const Ray& ray, float t); // 数値評価なので負のtも許可。
	std::optional<RayInterval> Intersect(const Ray& ray, const AABB& box); // 前方t>=0、接触含む。
	std::optional<float> Intersect(const Ray& ray, const Plane& plane); // 前方t>=0。平行/同一平面なら一意交点なし。
}
