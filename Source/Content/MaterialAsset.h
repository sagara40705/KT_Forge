#pragma once
#include <array>

namespace KT::Content
{
	// Rendererが描画方式を選ぶためのCPU設定。ShaderやPSOは保持しない。
	enum class MaterialModel
	{
		UnlitOpaque
	};

	// 線形RGBAを保持する不透明Material。alphaによる透過処理は行わない。
	class MaterialAsset
	{
	public:
		explicit MaterialAsset(std::array<float, 4> baseColor = {1, 1, 1, 1}, MaterialModel model = MaterialModel::UnlitOpaque);
		[[nodiscard]] const std::array<float, 4>& BaseColor() const noexcept;
		[[nodiscard]] MaterialModel Model() const noexcept;

	private:
		std::array<float, 4> baseColor_;
		MaterialModel model_;
	};
}
