#pragma once
#include <cmath>

namespace KT::Core::Math
{
	inline bool IsFinite(float value) noexcept
	{
		return std::isfinite(value);
	}

	// 非有限値/不正な許容値はfalse。abs/relativeの大きい方を許容誤差に使う。
	bool Near(float first, float second, float absoluteTolerance = 1e-5f, float relativeTolerance = 1e-5f) noexcept;
	float Clamp(float value, float minimum, float maximum); // 有限、min<=maxが必要。

	namespace Detail
	{
		// 全計算でfloatへの縮小前に範囲を確認する。公開演算の実装用。
		float CheckedFloat(double value);
	}
}
