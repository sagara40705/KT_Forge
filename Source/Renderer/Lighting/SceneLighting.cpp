#include <Renderer/Lighting/SceneLighting.h>
#include <cmath>
#include <stdexcept>

namespace KT::Renderer
{
	KT::Core::Math::Matrix4 DirectionalShadowMatrix(
		KT::Core::Math::Vector3 direction, KT::Core::Math::Vector3 center, float extent, float depth, unsigned resolution)
	{
		using namespace KT::Core::Math;
		if (!IsFinite(center) || !std::isfinite(extent) || extent < 0.1f || extent > 10000 || !std::isfinite(depth) || depth < 0.1f ||
			depth > 10000 || resolution < 128 || resolution > 4096 || (resolution & (resolution - 1)) != 0)
		{
			throw std::invalid_argument("Directional shadowの中心・範囲・解像度が不正です。");
		}
		const auto forward = Normalized(direction);
		const auto up = std::abs(forward.y) > 0.99f ? Vector3{0, 0, 1} : Vector3{0, 1, 0};
		const auto right = Normalized(Cross(up, forward));
		const auto vertical = Cross(forward, right);
		// 光源平面でtexel単位に中心を丸め、ゲーム追従時の細かい揺れを抑える。
		const double texel = double(extent) * 2 / resolution;
		const double horizontalCenter = std::round(double(Dot(center, right)) / texel) * texel;
		const double verticalCenter = std::round(double(Dot(center, vertical)) / texel) * texel;
		Matrix4 result;
		result(0, 0) = right.x / extent;
		result(1, 0) = right.y / extent;
		result(2, 0) = right.z / extent;
		result(3, 0) = static_cast<float>(-horizontalCenter / extent);
		result(0, 1) = vertical.x / extent;
		result(1, 1) = vertical.y / extent;
		result(2, 1) = vertical.z / extent;
		result(3, 1) = static_cast<float>(-verticalCenter / extent);
		result(0, 2) = -forward.x / (2 * depth);
		result(1, 2) = -forward.y / (2 * depth);
		result(2, 2) = -forward.z / (2 * depth);
		result(3, 2) = 0.5f + Dot(center, forward) / (2 * depth);
		if (!IsFinite(result))
		{
			throw std::overflow_error("Directional shadowの行列を有限floatで表現できません。");
		}
		return result;
	}

	void SceneLighting::Validate() const
	{
		using namespace KT::Core::Math;
		if (constants.shadowEnabled != (shadows ? 1.0f : 0.0f) || !IsAffine(constants.shadowViewProjection) ||
			!IsFinite(constants.direction) || std::abs(Length(constants.direction) - 1) > 0.001f || shadowResolution < 128 ||
			shadowResolution > 4096 || (shadowResolution & (shadowResolution - 1)) != 0)
		{
			throw std::invalid_argument("描画用主光源の行列・単位方向・影解像度が不正です。");
		}
		for (const auto value : constants.color)
		{
			if (!std::isfinite(value) || value < 0 || value > 1)
			{
				throw std::invalid_argument("描画用主光源の色には0..1の有限値が必要です。");
			}
		}
		for (const auto value : constants.ambient)
		{
			if (!std::isfinite(value) || value < 0 || value > 1)
			{
				throw std::invalid_argument("描画用主光源の環境色には0..1の有限値が必要です。");
			}
		}
		if (!std::isfinite(constants.intensity) || constants.intensity < 0 || constants.intensity > 8 ||
			!std::isfinite(constants.depthBias) || constants.depthBias < 0 || constants.depthBias > 0.1f ||
			!std::isfinite(constants.normalBias) || constants.normalBias < 0 || constants.normalBias > 1 ||
			!std::isfinite(constants.shadowStrength) || constants.shadowStrength < 0 || constants.shadowStrength > 1)
		{
			throw std::invalid_argument("描画用主光源の強度・影bias・影strengthが不正です。");
		}
	}
}
