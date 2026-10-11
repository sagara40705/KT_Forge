#include <Content/MaterialAsset.h>
#include <cmath>
#include <stdexcept>

namespace KT::Content
{
	MaterialAsset::MaterialAsset(std::array<float, 4> baseColor, MaterialModel model)
		: baseColor_(baseColor),
		  model_(model)
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
