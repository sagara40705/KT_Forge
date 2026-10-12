#pragma once
#include <array>
#include <Content/AssetId.h>
#include <optional>

namespace KT::Content
{
	// Rendererが描画方式を選ぶためのCPU設定。ShaderやPSOは保持しない。
	enum class MaterialModel
	{
		Unlit,
		UnlitOpaque = Unlit,
		Toon
	};

	// 透過合成は行わず、Maskだけ閾値未満の画素を破棄する。
	enum class AlphaMode
	{
		Opaque,
		Mask
	};
	enum class TextureFilter
	{
		Point,
		Linear
	};
	enum class TextureAddress
	{
		Wrap,
		Clamp
	};

	// UVは左上原点。参照未割当では色係数だけを使用する。
	struct UnlitSettings
	{
		std::optional<TextureAssetId> baseColorTexture;
		std::array<float, 2> uvScale{1, 1};
		std::array<float, 2> uvOffset{0, 0};
		TextureFilter filter = TextureFilter::Linear;
		TextureAddress addressU = TextureAddress::Wrap;
		TextureAddress addressV = TextureAddress::Wrap;
		AlphaMode alphaMode = AlphaMode::Opaque;
		float alphaCutoff = 0.5f;
		bool operator==(const UnlitSettings&) const = default;
	};

	// 二段階の明暗境界と暗部の色係数。輪郭線や顔専用の影は含めない。
	struct ToonSettings
	{
		// NdotLを分ける境界と、その前後に補間する幅。
		float threshold = 0.5f;
		float smoothness = 0.04f;
		// 暗い側の光色へ掛ける線形RGB係数。
		std::array<float, 3> shadeColor{0.45f, 0.45f, 0.55f};
		bool receiveShadows = true;
		bool operator==(const ToonSettings&) const = default;
	};

	// 線形色係数と不変Forward設定。Maskによる切抜きは行うが、透過合成は行わない。
	class MaterialAsset
	{
	public:
		explicit MaterialAsset(std::array<float, 4> baseColor = {1, 1, 1, 1}, MaterialModel model = MaterialModel::Unlit,
			UnlitSettings settings = {}, ToonSettings toon = {});

		[[nodiscard]] const ToonSettings& Toon() const noexcept
		{
			return toon_;
		}
		[[nodiscard]] const std::array<float, 4>& BaseColor() const noexcept;
		[[nodiscard]] MaterialModel Model() const noexcept;

		[[nodiscard]] const UnlitSettings& Settings() const noexcept
		{
			return settings_;
		}

	private:
		std::array<float, 4> baseColor_;
		MaterialModel model_;
		UnlitSettings settings_;
		// Toonで使用する明暗設定。Unlitでは既定値だけを受理する。
		ToonSettings toon_;
	};
}
