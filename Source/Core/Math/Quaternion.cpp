#include <Core/Math/Quaternion.h>
#include <Core/Math/Scalar.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace KT::Core::Math
{
	bool IsFinite(Quaternion q) noexcept
	{
		return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w);
	}

	Quaternion Multiply(Quaternion a, Quaternion b)
	{
		if (!IsFinite(a) || !IsFinite(b))
		{
			throw std::invalid_argument("Quaternion product input must be finite.");
		}
		// Hamilton(b,a): row-vectorに合わせ、aを適用した後にbを適用する。
		return {Detail::CheckedFloat(double(b.w) * a.x + double(b.x) * a.w + double(b.y) * a.z - double(b.z) * a.y),
			Detail::CheckedFloat(double(b.w) * a.y - double(b.x) * a.z + double(b.y) * a.w + double(b.z) * a.x),
			Detail::CheckedFloat(double(b.w) * a.z + double(b.x) * a.y - double(b.y) * a.x + double(b.z) * a.w),
			Detail::CheckedFloat(double(b.w) * a.w - double(b.x) * a.x - double(b.y) * a.y - double(b.z) * a.z)};
	}

	Vector3 Rotate(Vector3 v, Quaternion rotation)
	{
		if (!IsFinite(v))
		{
			throw std::invalid_argument("Rotation vector must be finite.");
		}
		const auto q = Normalize(rotation);
		const double tx = 2 * (double(q.y) * v.z - double(q.z) * v.y);
		const double ty = 2 * (double(q.z) * v.x - double(q.x) * v.z);
		const double tz = 2 * (double(q.x) * v.y - double(q.y) * v.x);
		return {Detail::CheckedFloat(v.x + double(q.w) * tx + double(q.y) * tz - double(q.z) * ty),
			Detail::CheckedFloat(v.y + double(q.w) * ty + double(q.z) * tx - double(q.x) * tz),
			Detail::CheckedFloat(v.z + double(q.w) * tz + double(q.x) * ty - double(q.y) * tx)};
	}

	Quaternion Slerp(Quaternion first, Quaternion second, float t)
	{
		if (!IsFinite(t) || t < 0 || t > 1)
		{
			throw std::invalid_argument("Slerp t must be in [0,1].");
		}
		const auto a = Normalize(first);
		auto b = Normalize(second);
		double dot = double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z + double(a.w) * b.w;
		if (dot < 0)
		{
			b = -b;
			dot = -dot;
		}
		dot = std::clamp(dot, -1.0, 1.0);
		double wa = 1 - double(t), wb = t;
		if (dot < 0.9995)
		{
			const double angle = std::acos(dot), sine = std::sin(angle);
			wa = std::sin((1 - double(t)) * angle) / sine;
			wb = std::sin(double(t) * angle) / sine;
		}
		return Normalize({float(wa * a.x + wb * b.x), float(wa * a.y + wb * b.y), float(wa * a.z + wb * b.z), float(wa * a.w + wb * b.w)});
	}

	Quaternion Normalize(Quaternion q)
	{
		const double length = std::hypot(std::hypot(double(q.x), double(q.y)), std::hypot(double(q.z), double(q.w)));
		if (!std::isfinite(length) || length == 0)
		{
			throw std::invalid_argument("Quaternionは有限かつ非zeroである必要があります。");
		}
		return {float(q.x / length), float(q.y / length), float(q.z / length), float(q.w / length)};
	}

	Quaternion FromAxisAngle(Vector3 axis, float radians)
	{
		const double length = std::hypot(double(axis.x), double(axis.y), double(axis.z));
		if (!std::isfinite(length) || length == 0 || !std::isfinite(radians))
		{
			throw std::invalid_argument("回転軸/角度が不正です。角度はradianです。");
		}
		const double sine = std::sin(double(radians) * 0.5);
		return Normalize({float(axis.x / length * sine), float(axis.y / length * sine), float(axis.z / length * sine),
			float(std::cos(double(radians) * 0.5))});
	}
}
