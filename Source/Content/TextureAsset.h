#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace KT::Content
{
	// RGBだけの転送関数。alphaは常に線形のstraight alphaを使用する。
	enum class TextureColorSpace
	{
		Linear,
		Srgb
	};
	// Generateは元のMipを捨てて全段を生成し、Preserveは入力の段数を保つ。
	enum class TextureMipMode
	{
		Generate,
		Preserve
	};

	struct TextureMip
	{
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		// 左上原点、行間paddingなしのRGBA8。
		std::vector<std::uint8_t> pixels;
	};

	// JSONでは定義だけを保持する。Catalog登録時には検証済み画素を必須とする。
	class TextureAsset
	{
	public:
		static constexpr std::size_t MaxDecodedBytes = 256 * 1024 * 1024;
		TextureAsset(std::string source, TextureColorSpace colorSpace = TextureColorSpace::Srgb,
			TextureMipMode mipMode = TextureMipMode::Generate, std::vector<TextureMip> mips = {});

		[[nodiscard]] const std::string& Source() const noexcept
		{
			return source_;
		}

		[[nodiscard]] TextureColorSpace ColorSpace() const noexcept
		{
			return colorSpace_;
		}

		[[nodiscard]] TextureMipMode MipMode() const noexcept
		{
			return mipMode_;
		}

		[[nodiscard]] std::span<const TextureMip> Mips() const noexcept;
		void ValidateLoaded() const;

	private:
		std::string source_;
		TextureColorSpace colorSpace_;
		TextureMipMode mipMode_;
		// Catalog一括登録の候補コピーで大きな画素列を複製しない。
		std::shared_ptr<const std::vector<TextureMip>> mips_;
	};
}
