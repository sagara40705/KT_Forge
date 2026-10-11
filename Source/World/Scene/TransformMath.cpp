#include <World/Scene/TransformMath.h>
#include <Core/Math/Scalar.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace KT::World
{
	// 行ごとの大きさで誤差を測り、大きな平行移動で回転の誤差を隠さない。
	bool TransformNear(const KT::Core::Math::Matrix4& first, const KT::Core::Math::Matrix4& second)
	{
		for (std::size_t row = 0; row < 4; ++row)
		{
			double magnitude = 0;
			for (std::size_t column = 0; column < 3; ++column)
			{
				magnitude = (std::max)({magnitude, std::abs(double(first(row, column))), std::abs(double(second(row, column)))});
			}

			for (std::size_t column = 0; column < 3; ++column)
			{
				// 平行移動は各成分で比較し、原点への相殺誤差には絶対許容値を使う。
				const auto translationMagnitude = (std::max)(std::abs(double(first(row, column))), std::abs(double(second(row, column))));
				const auto tolerance = row == 3 ? (std::max)(1.0e-5, 2.0e-5 * translationMagnitude) : 2.0e-5 * magnitude;
				if (std::abs(double(first(row, column)) - second(row, column)) > tolerance)
				{
					return false;
				}
			}
		}
		return true;
	}

	// S*R*Tの行基底を分解する。失われた回転軸は補完し、shearは近似しない。
	LocalTransform DecomposeLocalTransform(const KT::Core::Math::Matrix4& matrix)
	{
		using KT::Core::Math::Detail::CheckedFloat;
		if (!KT::Core::Math::IsAffine(matrix))
		{
			throw std::invalid_argument("KeepWorldの新Local行列が有限のaffine行列ではありません。");
		}

		std::array<std::array<double, 3>, 3> axes{};
		std::array<double, 3> scales{};
		std::size_t presentCount = 0;
		std::size_t presentAxis = 0;
		std::size_t missingAxis = 0;
		for (std::size_t row = 0; row < 3; ++row)
		{
			scales[row] = std::hypot(double(matrix(row, 0)), double(matrix(row, 1)), double(matrix(row, 2)));
			if (scales[row] == 0)
			{
				missingAxis = row;
				continue;
			}

			++presentCount;
			presentAxis = row;
			for (std::size_t column = 0; column < 3; ++column)
			{
				axes[row][column] = matrix(row, column) / scales[row];
			}
		}

		const auto cross = [](const auto& first, const auto& second) -> std::array<double, 3>
		{
			return {first[1] * second[2] - first[2] * second[1], first[2] * second[0] - first[0] * second[2],
				first[0] * second[1] - first[1] * second[0]};
		};
		const auto dot = [](const auto& first, const auto& second)
		{ return first[0] * second[0] + first[1] * second[1] + first[2] * second[2]; };

		// 非zeroの行が直交しない場合、LocalTransformでは表現できない。
		for (std::size_t first = 0; first < 3; ++first)
		{
			for (std::size_t second = first + 1; second < 3; ++second)
			{
				if (scales[first] != 0 && scales[second] != 0 && std::abs(dot(axes[first], axes[second])) > 1.0e-5)
				{
					throw std::invalid_argument("KeepWorldの新Local行列にshearがあり、位置・回転・scaleで表現できません。");
				}
			}
		}

		// zero scaleの行を正の行列式の直交基底で補完する。残る行列の値は変えない。
		if (presentCount == 0)
		{
			axes = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
		}
		else if (presentCount == 1)
		{
			const auto& known = axes[presentAxis];
			const auto helperAxis = static_cast<std::size_t>(std::min_element(known.begin(), known.end(), [](double first, double second)
																 { return std::abs(first) < std::abs(second); }) -
				known.begin());
			std::array<double, 3> helper{};
			helper[helperAxis] = 1;
			const auto nextAxis = (presentAxis + 1) % 3;
			axes[nextAxis] = cross(helper, known);
			const auto length = std::hypot(axes[nextAxis][0], axes[nextAxis][1], axes[nextAxis][2]);
			for (auto& value : axes[nextAxis])
			{
				value /= length;
			}
			axes[(presentAxis + 2) % 3] = cross(known, axes[nextAxis]);
		}
		else if (presentCount == 2)
		{
			axes[missingAxis] = cross(axes[(missingAxis + 1) % 3], axes[(missingAxis + 2) % 3]);
		}
		else if (dot(axes[0], cross(axes[1], axes[2])) < 0)
		{
			// 反転は最大scale軸の符号へ移し、Quaternionには純粋な回転を渡す。
			const auto reflectedAxis = static_cast<std::size_t>(std::max_element(scales.begin(), scales.end()) - scales.begin());
			scales[reflectedAxis] = -scales[reflectedAxis];
			for (auto& value : axes[reflectedAxis])
			{
				value = -value;
			}
		}

		// row-vectorの回転行列から、最大成分を使ってQuaternionを安定に求める。
		std::array<double, 4> rotation{};
		const auto trace = axes[0][0] + axes[1][1] + axes[2][2];
		if (trace > 0)
		{
			const auto divisor = 2 * std::sqrt(trace + 1);
			rotation = {
				(axes[1][2] - axes[2][1]) / divisor, (axes[2][0] - axes[0][2]) / divisor, (axes[0][1] - axes[1][0]) / divisor, divisor / 4};
		}
		else
		{
			std::size_t largestAxis = 0;
			for (std::size_t axis = 1; axis < 3; ++axis)
			{
				if (axes[axis][axis] > axes[largestAxis][largestAxis])
				{
					largestAxis = axis;
				}
			}
			const auto nextAxis = (largestAxis + 1) % 3;
			const auto lastAxis = (largestAxis + 2) % 3;
			const auto divisor = 2 * std::sqrt(1 + axes[largestAxis][largestAxis] - axes[nextAxis][nextAxis] - axes[lastAxis][lastAxis]);
			rotation[largestAxis] = divisor / 4;
			rotation[nextAxis] = (axes[largestAxis][nextAxis] + axes[nextAxis][largestAxis]) / divisor;
			rotation[lastAxis] = (axes[largestAxis][lastAxis] + axes[lastAxis][largestAxis]) / divisor;
			rotation[3] = (axes[nextAxis][lastAxis] - axes[lastAxis][nextAxis]) / divisor;
		}

		// Cameraのunit scale制約も使えるよう、丸め誤差範囲の+1を正確な1へそろえる。
		for (auto& scale : scales)
		{
			if (std::abs(scale - 1) <= 1.0e-6)
			{
				scale = 1;
			}
		}
		LocalTransform local;
		local.position = {matrix(3, 0), matrix(3, 1), matrix(3, 2)};
		local.scale = {CheckedFloat(scales[0]), CheckedFloat(scales[1]), CheckedFloat(scales[2])};
		local.rotation = KT::Core::Math::Normalize(
			{CheckedFloat(rotation[0]), CheckedFloat(rotation[1]), CheckedFloat(rotation[2]), CheckedFloat(rotation[3])});

		// 分解後のfloat値から再構成し、近似では保持できない行列を拒否する。
		if (!TransformNear(matrix, KT::Core::Math::LocalMatrix(local.position, local.rotation, local.scale)))
		{
			throw std::invalid_argument("KeepWorldの新Local行列をTRSで許容誤差内に再構成できません。");
		}
		return local;
	}

}
