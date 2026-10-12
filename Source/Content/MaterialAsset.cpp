#include <Content/MaterialAsset.h>
#include <cmath>
#include <stdexcept>

namespace KT::Content
{
	MaterialAsset::MaterialAsset(std::array<float, 4> baseColor, MaterialModel model, UnlitSettings settings, ToonSettings toon)
		: baseColor_(baseColor),
		  model_(model),
		  settings_(std::move(settings)),
		  toon_(toon)
	{
		if (model_ != MaterialModel::Unlit && model_ != MaterialModel::Toon)
		{
			throw std::invalid_argument("MaterialAssetの描画方式が未対応です。");
		}
		// 保存可能なトゥーン設定を先に確認する。
		if (!std::isfinite(toon.threshold) || toon.threshold < 0 || toon.threshold > 1 || !std::isfinite(toon.smoothness) ||
			toon.smoothness < 0 || toon.smoothness > 1)
		{
			throw std::invalid_argument("Toonの明暗境界と滑らかさには0以上1以下の有限値が必要です。");
		}
		for (const auto value : toon.shadeColor)
		{
			if (!std::isfinite(value) || value < 0 || value > 1)
			{
				throw std::invalid_argument("Toonの暗部色には0以上1以下の有限RGBが必要です。");
			}
		}
		if (model == MaterialModel::Unlit && toon != ToonSettings{})
		{
			throw std::invalid_argument("Unlitには既定以外のToon設定を指定できません。");
		}
		for (const auto value : baseColor_)
		{
			if (!std::isfinite(value) || value < 0 || value > 1)
			{
				throw std::invalid_argument("MaterialAssetの線形RGBAは0以上1以下の有限値が必要です。");
			}
		}
		for (const auto value : {settings_.uvScale[0], settings_.uvScale[1], settings_.uvOffset[0], settings_.uvOffset[1]})
		{
			if (!std::isfinite(value))
			{
				throw std::invalid_argument("MaterialAssetのUV変換には有限値が必要です。");
			}
		}
		if ((settings_.filter != TextureFilter::Point && settings_.filter != TextureFilter::Linear) ||
			(settings_.addressU != TextureAddress::Wrap && settings_.addressU != TextureAddress::Clamp) ||
			(settings_.addressV != TextureAddress::Wrap && settings_.addressV != TextureAddress::Clamp))
		{
			throw std::invalid_argument("MaterialAssetのSampler設定が未対応です。");
		}
		if ((settings_.alphaMode != AlphaMode::Opaque && settings_.alphaMode != AlphaMode::Mask) || !std::isfinite(settings_.alphaCutoff) ||
			settings_.alphaCutoff < 0 || settings_.alphaCutoff > 1)
		{
			throw std::invalid_argument("MaterialAssetのAlpha設定にはOpaque/Maskと0以上1以下の閾値が必要です。");
		}
	}

	const std::array<float, 4>& MaterialAsset::BaseColor() const noexcept
	{
		return baseColor_;
	}

	MaterialModel MaterialAsset::Model() const noexcept
	{
		return model_;
	}
}
