#pragma once
#include <Core/Math/Vector3.h>

namespace KT::Core::Math
{
	struct Plane
	{
		Vector3 normal{0,1,0};
		float distance=0; // dot(normal,point)+distance=0、内側は>=0。
	};
	bool IsValid(const Plane& plane) noexcept; // finite、normalの長さが1（許容差1e-5）。
	Plane PlaneFromCoefficients(double x, double y, double z, double distance);
	Plane PlaneFromPointNormal(Vector3 point, Vector3 normal);
	Plane Normalized(const Plane& plane); // finite/nonzero normalを正規化、distanceも同じ比率で割る。
	float SignedDistance(const Plane& plane, Vector3 point);
	Vector3 ProjectOntoPlane(Vector3 point, const Plane& plane);
}
