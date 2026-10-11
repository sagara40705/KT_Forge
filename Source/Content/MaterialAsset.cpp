#include <Content/MaterialAsset.h>
#include <cmath>
#include <stdexcept>

namespace KT::Content
{
	MaterialAsset::MaterialAsset(std::array<float, 4> baseColor, MaterialModel model, UnlitSettings settings)
		: baseColor_(baseColor),
		  model_(model),
		  settings_(std::move(settings))
	{
		if (model_ != MaterialModel::UnlitOpaque)
		{
			throw std::invalid_argument("MaterialAssetの描画方式が未対応です。");
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
