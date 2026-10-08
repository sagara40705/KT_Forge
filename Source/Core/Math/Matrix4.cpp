#include <Core/Math/Matrix4.h>
#include <Core/Math/Scalar.h>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace KT::Core::Math
{
	using Detail::CheckedFloat;
	bool IsAffine(const Matrix4& m) noexcept
	{
		return IsFinite(m) && m(0,3)==0 && m(1,3)==0 && m(2,3)==0 && m(3,3)==1;
	}
	Matrix4 Translation(Vector3 position)
	{
		if (!IsFinite(position)) throw std::invalid_argument("Translation must be finite.");
		Matrix4 result;
		result(3,0)=position.x; result(3,1)=position.y; result(3,2)=position.z;
		return result;
	}
	Matrix4 Scale(Vector3 scale)
	{
		if (!IsFinite(scale)) throw std::invalid_argument("Scale must be finite.");
		Matrix4 result;
		result(0,0)=scale.x; result(1,1)=scale.y; result(2,2)=scale.z;
		return result;
	}
	Matrix4 ToMatrix(Quaternion rotation) { return LocalMatrix({},rotation,{1,1,1}); }
	Matrix4 Transpose(const Matrix4& m) noexcept
	{
		Matrix4 result;
		for (std::size_t r=0; r<4; ++r) for (std::size_t c=0; c<4; ++c) result(r,c)=m(c,r);
		return result;
	}
	Vector4 Transform(Vector4 v, const Matrix4& m)
	{
		if (!IsFinite(v) || !IsFinite(m)) throw std::invalid_argument("Transform input must be finite.");
		const auto component = [&](std::size_t c) {
			return CheckedFloat(double(v.x)*m(0,c)+double(v.y)*m(1,c)+double(v.z)*m(2,c)+double(v.w)*m(3,c));
		};
		return {component(0),component(1),component(2),component(3)};
	}
	Vector3 TransformPoint(Vector3 v, const Matrix4& m)
	{
		if (!IsAffine(m)) throw std::invalid_argument("TransformPoint requires an affine matrix.");
		const auto result = Transform({v.x,v.y,v.z,1},m);
		return {result.x,result.y,result.z};
	}
	Vector3 TransformVector(Vector3 v, const Matrix4& m)
	{
		if (!IsAffine(m)) throw std::invalid_argument("TransformVector requires an affine matrix.");
		const auto result = Transform({v.x,v.y,v.z,0},m);
		return {result.x,result.y,result.z};
	}
	Vector3 ProjectPoint(Vector3 v, const Matrix4& m)
	{
		if (!IsFinite(v) || !IsFinite(m)) throw std::invalid_argument("Projection input must be finite.");
		const auto component = [&](std::size_t c) {
			return double(v.x)*m(0,c)+double(v.y)*m(1,c)+double(v.z)*m(2,c)+m(3,c);
		};
		const double w = component(3);
		if (w == 0) throw std::invalid_argument("Projection homogeneous w is zero.");
		return {CheckedFloat(component(0)/w),CheckedFloat(component(1)/w),CheckedFloat(component(2)/w)};
	}
	bool IsFinite(const Matrix4& m) noexcept
	{
		return std::all_of(m.elements.begin(), m.elements.end(), [](float x) { return std::isfinite(x); });
	}
	Matrix4 Multiply(const Matrix4& a, const Matrix4& b)
	{
		if (!IsFinite(a) || !IsFinite(b)) throw std::invalid_argument("行列が非有限です。");
		Matrix4 result;
		for (std::size_t r = 0; r < 4; ++r)
			for (std::size_t c = 0; c < 4; ++c)
			{
				double value = 0;
				for (std::size_t k = 0; k < 4; ++k) value += double(a(r,k)) * b(k,c);
				result(r,c) = CheckedFloat(value);
			}
		if (!IsFinite(result)) throw std::overflow_error("行列積がfloatで表現できません。");
		return result;
	}
	Matrix4 LocalMatrix(Vector3 position, Quaternion rotation, Vector3 scale)
	{
		if (!IsFinite(position) || !IsFinite(scale)) throw std::invalid_argument("位置/scaleが非有限です。");
		const auto q = Normalize(rotation);
		Matrix4 r{{1-2*(q.y*q.y+q.z*q.z), 2*(q.x*q.y+q.z*q.w), 2*(q.x*q.z-q.y*q.w), 0,
			2*(q.x*q.y-q.z*q.w), 1-2*(q.x*q.x+q.z*q.z), 2*(q.y*q.z+q.x*q.w), 0,
			2*(q.x*q.z+q.y*q.w), 2*(q.y*q.z-q.x*q.w), 1-2*(q.x*q.x+q.y*q.y), 0, 0,0,0,1}};
		for (std::size_t c = 0; c < 3; ++c) { r(0,c) *= scale.x; r(1,c) *= scale.y; r(2,c) *= scale.z; }
		r(3,0) = position.x; r(3,1) = position.y; r(3,2) = position.z;
		if (!IsFinite(r)) throw std::overflow_error("Local行列がfloatで表現できません。");
		return r;
	}
	Matrix4 Inverse(const Matrix4& m)
	{
		if (!IsFinite(m)) throw std::invalid_argument("逆行列入力が非有限です。");
		double a[4][8]{}, scales[4]{};
		for (std::size_t r=0; r<4; ++r)
		{
			for (std::size_t c=0; c<4; ++c) { a[r][c]=m(r,c); scales[r]=std::max(scales[r],std::abs(a[r][c])); }
			a[r][r+4]=1;
			if (scales[r]==0) throw std::invalid_argument("行列が特異です。");
		}
		for (std::size_t c=0; c<4; ++c)
		{
			std::size_t pivot=c;
			for (std::size_t r=c+1; r<4; ++r)
				if (std::abs(a[r][c])/scales[r] > std::abs(a[pivot][c])/scales[pivot]) pivot=r;
			if (std::abs(a[pivot][c])/scales[pivot] <= 1.0e-10) throw std::invalid_argument("行列が特異/数値的に不安定です。");
			for (std::size_t k=0; k<8; ++k) std::swap(a[c][k],a[pivot][k]);
			std::swap(scales[c],scales[pivot]);
			const double divisor=a[c][c];
			for (double& value:a[c]) value/=divisor;
			for (std::size_t r=0; r<4; ++r)
				if (r!=c) { const double factor=a[r][c]; for (std::size_t k=0; k<8; ++k) a[r][k]-=factor*a[c][k]; }
		}
		Matrix4 result;
		for (std::size_t r=0; r<4; ++r) for (std::size_t c=0; c<4; ++c) result(r,c)=CheckedFloat(a[r][c+4]);
		if (!IsFinite(result)) throw std::overflow_error("逆行列がfloatで表現できません。");
		return result;
	}
	Matrix4 PerspectiveReverseZ(float fov, float aspect, float nearPlane, float farPlane)
	{
		if (!std::isfinite(fov) || fov<=0 || fov>=std::numbers::pi_v<float> || !std::isfinite(aspect) || aspect<=0 ||
			!std::isfinite(nearPlane) || !std::isfinite(farPlane) || nearPlane<=0 || farPlane<=nearPlane)
			throw std::invalid_argument("透視投影設定が不正です。");
		const double y=1/std::tan(double(fov)*0.5), range=double(farPlane)-nearPlane;
		Matrix4 result{{}};
		result(0,0)=CheckedFloat(y/aspect); result(1,1)=CheckedFloat(y); result(2,2)=CheckedFloat(-double(nearPlane)/range);
		result(2,3)=1; result(3,2)=CheckedFloat(double(nearPlane)*farPlane/range);
		if (!IsFinite(result) || result(0,0)<=0 || result(1,1)<=0 || result(2,2)>=0 || result(3,2)<=0)
			throw std::overflow_error("投影行列がfloatで表現できません。");
		return result;
	}
}
