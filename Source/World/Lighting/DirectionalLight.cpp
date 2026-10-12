#include <World/Lighting/DirectionalLight.h>
#include <World/Scene/ComponentRegistry.h>
#include <cmath>
#include <stdexcept>

namespace KT::World
{
	void DirectionalLight::Validate() const
	{
		using namespace KT::Core::Math;
		if (!IsFinite(direction) || (direction.x == 0 && direction.y == 0 && direction.z == 0))
		{
			throw std::invalid_argument("DirectionalLightのWorld方向には有限の非zero Vector3が必要です。");
		}
		(void)Normalized(direction);
		for (const auto& values : {color, ambient})
		{
			for (const auto value : values)
			{
				if (!std::isfinite(value) || value < 0 || value > 1)
				{
					throw std::invalid_argument("DirectionalLightの光色と環境色には0以上1以下の有限RGBが必要です。");
				}
			}
		}
		if (!std::isfinite(intensity) || intensity < 0 || intensity > 8)
		{
			throw std::invalid_argument("DirectionalLightの強度には0以上8以下の有限値が必要です。");
		}
		if (!IsFinite(shadowCenter) || !std::isfinite(shadowExtent) || shadowExtent < 0.1f || shadowExtent > 10000 ||
			!std::isfinite(shadowDepth) || shadowDepth < 0.1f || shadowDepth > 10000)
		{
			throw std::invalid_argument("影の中心には有限値、半幅と奥行には0.1以上10000以下の有限値が必要です。");
		}
		if (shadowResolution < 128 || shadowResolution > 4096 || (shadowResolution & (shadowResolution - 1)) != 0)
		{
			throw std::invalid_argument("影の解像度には128以上4096以下の2の累乗が必要です。");
		}
		if (!std::isfinite(depthBias) || depthBias < 0 || depthBias > 0.1f || !std::isfinite(normalBias) || normalBias < 0 ||
			normalBias > 1 || !std::isfinite(shadowStrength) || shadowStrength < 0 || shadowStrength > 1)
		{
			throw std::invalid_argument("影のdepthBiasは0..0.1、normalBiasとstrengthは0..1の有限値が必要です。");
		}
	}

	void RegisterDirectionalLightComponent(ComponentRegistry& registry)
	{
		const auto vector = [](const SceneValue& value)
		{
			const auto& a = value.AsArray();
			if (a.size() != 3)
			{
				throw std::invalid_argument("DirectionalLightのVector3には3要素が必要です。");
			}
			return KT::Core::Math::Vector3{a[0].AsFloat(), a[1].AsFloat(), a[2].AsFloat()};
		};
		registry.Register<DirectionalLight>(
			"kt.DirectionalLight", 1,
			[](const DirectionalLight& input) -> SceneValue
			{
				input.Validate();
				const auto v = [](auto x) -> SceneValue { return SceneValue::Array{double(x.x), double(x.y), double(x.z)}; };
				const auto c = [](auto x) -> SceneValue { return SceneValue::Array{double(x[0]), double(x[1]), double(x[2])}; };
				return SceneValue::Object{{"direction", v(input.direction)}, {"color", c(input.color)}, {"ambient", c(input.ambient)},
					{"intensity", double(input.intensity)}, {"enabled", input.enabled}, {"shadows", input.shadows},
					{"shadowCenter", v(input.shadowCenter)}, {"shadowExtent", double(input.shadowExtent)},
					{"shadowDepth", double(input.shadowDepth)}, {"shadowResolution", double(input.shadowResolution)},
					{"depthBias", double(input.depthBias)}, {"normalBias", double(input.normalBias)},
					{"shadowStrength", double(input.shadowStrength)}};
			},
			[vector](const SceneValue& data, const SceneReadContext&) -> DirectionalLight
			{
				data.RequireFields({"direction", "color", "ambient", "intensity", "enabled", "shadows", "shadowCenter", "shadowExtent",
					"shadowDepth", "shadowResolution", "depthBias", "normalBias", "shadowStrength"});
				DirectionalLight result;
				result.direction = vector(data.At("direction"));
				const auto color = vector(data.At("color"));
				const auto ambient = vector(data.At("ambient"));
				result.color = {color.x, color.y, color.z};
				result.ambient = {ambient.x, ambient.y, ambient.z};
				result.intensity = data.At("intensity").AsFloat();
				result.enabled = data.At("enabled").AsBool();
				result.shadows = data.At("shadows").AsBool();
				result.shadowCenter = vector(data.At("shadowCenter"));
				result.shadowExtent = data.At("shadowExtent").AsFloat();
				result.shadowDepth = data.At("shadowDepth").AsFloat();
				const auto resolution = data.At("shadowResolution").AsNumber();
				if (resolution < 128 || resolution > 4096 || std::floor(resolution) != resolution)
				{
					throw std::invalid_argument("影の解像度は128..4096の整数で保存してください。");
				}
				result.shadowResolution = static_cast<unsigned>(resolution);
				result.depthBias = data.At("depthBias").AsFloat();
				result.normalBias = data.At("normalBias").AsFloat();
				result.shadowStrength = data.At("shadowStrength").AsFloat();
				result.Validate();
				return result;
			});
	}
}
