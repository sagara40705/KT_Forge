#pragma once
#include <Core/Math/Quaternion.h>
#include <Core/Math/Vector4.h>
#include <array>
#include <cstddef>

namespace KT::Core::Math
{
	// row-major格納/row-vector演算。HLSLはrow_majorとmul(position,matrix)、転置なし。
	struct Matrix4
	{
		std::array<float,16> elements{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
		// row/columnは0..3。uncheckedアクセスなので呼出側が範囲を守る。
		float& operator()(std::size_t row, std::size_t column) noexcept { return elements[row*4+column]; }
		float operator()(std::size_t row, std::size_t column) const noexcept { return elements[row*4+column]; }
	};
	constexpr Matrix4 Identity() noexcept { return {}; }
	bool IsFinite(const Matrix4& matrix) noexcept;
	bool IsAffine(const Matrix4& matrix) noexcept;
	Matrix4 Translation(Vector3 position);
	Matrix4 Scale(Vector3 scale); // zero/負scaleも一般の数値変換として許可。
	Matrix4 ToMatrix(Quaternion rotation);
	Matrix4 LocalMatrix(Vector3 position, Quaternion rotation, Vector3 scale); // 既存API、S*R*T。
	inline Matrix4 TRS(Vector3 scale, Quaternion rotation, Vector3 position) { return LocalMatrix(position,rotation,scale); }
	Matrix4 Multiply(const Matrix4& first, const Matrix4& second);
	inline Matrix4 operator*(const Matrix4& first, const Matrix4& second) { return Multiply(first,second); }
	Matrix4 Transpose(const Matrix4& matrix) noexcept;
	Matrix4 Inverse(const Matrix4& matrix); // 特異/数値的不安定/表現不能を例外で拒否。
	Vector4 Transform(Vector4 value, const Matrix4& matrix);
	inline Vector4 operator*(Vector4 value, const Matrix4& matrix) { return Transform(value,matrix); }
	Vector3 TransformPoint(Vector3 point, const Matrix4& matrix); // affine専用、w=1。
	Vector3 TransformVector(Vector3 vector, const Matrix4& matrix); // affine専用、w=0。正規化しない。
	Vector3 ProjectPoint(Vector3 point, const Matrix4& matrix); // 同次除算、w!=0。背面/clip内外判定は別。
	// LH/+Zforward、有限farのReverse-Z。near→1、far→0、clip.w=view.z。
	Matrix4 PerspectiveReverseZ(float verticalFovRadians, float aspect, float nearPlane, float farPlane);
	inline Matrix4 PerspectiveReverseZ(Radians fov, float aspect, float nearPlane, float farPlane)
	{
		return PerspectiveReverseZ(fov.value,aspect,nearPlane,farPlane);
	}
}
