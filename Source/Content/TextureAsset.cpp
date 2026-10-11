#include <Content/TextureAsset.h>
#include <algorithm>
#include <stdexcept>

namespace KT::Content
{
	TextureAsset::TextureAsset(std::string source, TextureColorSpace colorSpace, TextureMipMode mipMode, std::vector<TextureMip> mips)
		: source_(std::move(source)),
		  colorSpace_(colorSpace),
		  mipMode_(mipMode)
	{
		if (source_.empty() || source_.find('\0') != std::string::npos)
		{
			throw std::invalid_argument("Texture素材には空でない画像の相対パスが必要です。");
		}
		if ((colorSpace != TextureColorSpace::Linear && colorSpace != TextureColorSpace::Srgb) ||
			(mipMode != TextureMipMode::Generate && mipMode != TextureMipMode::Preserve))
		{
			throw std::invalid_argument("Texture素材の色空間またはMip方式が未対応です。");
		}
		if (!mips.empty())
		{
			mips_ = std::make_shared<const std::vector<TextureMip>>(std::move(mips));
			ValidateLoaded();
		}
	}

	std::span<const TextureMip> TextureAsset::Mips() const noexcept
	{
		return mips_ ? std::span<const TextureMip>(*mips_) : std::span<const TextureMip>{};
	}

	void TextureAsset::ValidateLoaded() const
	{
		if (Mips().empty())
		{
			throw std::invalid_argument("Texture素材の画素が読み込まれていません。");
		}
		auto width = Mips().front().width;
		auto height = Mips().front().height;
		if (width == 0 || height == 0 || width > 16384 || height > 16384)
		{
			throw std::invalid_argument("Texture素材の縦横は1から16384の範囲が必要です。");
		}
		std::size_t total = 0;
		for (std::size_t index = 0; index < Mips().size(); ++index)
		{
			const auto& mip = Mips()[index];
			const auto bytes = static_cast<std::size_t>(width) * height * 4;
			if (mip.width != width || mip.height != height || mip.pixels.size() != bytes || bytes > MaxDecodedBytes - total)
			{
				throw std::invalid_argument("Texture素材のMip寸法・RGBA8 byte数または合計256MiBの上限が不正です。");
			}
			total += bytes;
			if (width == 1 && height == 1 && index + 1 != Mips().size())
			{
				throw std::invalid_argument("Texture素材に1x1より後のMipがあります。");
			}
			width = (std::max)(1u, width / 2);
			height = (std::max)(1u, height / 2);
		}
		if (mipMode_ == TextureMipMode::Generate && (Mips().back().width != 1 || Mips().back().height != 1))
		{
			throw std::invalid_argument("Generate指定のTexture素材には1x1までのMipが必要です。");
		}
	}
}
