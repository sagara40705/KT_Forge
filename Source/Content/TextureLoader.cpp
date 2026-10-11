#include <Content/TextureLoader.h>
#include <DirectXTex.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace KT::Content
{
	static void CheckImageResult(HRESULT result, const char* operation)
	{
		if (FAILED(result))
		{
			throw std::runtime_error(std::string("Texture画像の") + operation + "に失敗しました。HRESULT=" + std::to_string(result));
		}
	}

	// 呼出threadのCOM初期化だけを所有し、既存のapartmentを変更しない。
	struct ImageComScope
	{
		HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

		ImageComScope()
		{
			if (result != RPC_E_CHANGED_MODE)
			{
				CheckImageResult(result, "COM初期化");
			}
		}

		~ImageComScope()
		{
			if (SUCCEEDED(result))
			{
				CoUninitialize();
			}
		}
	};

	TextureAsset TextureLoader::Load(const TextureAsset& definition, const std::filesystem::path& path)
	{
		ImageComScope com;
		std::ifstream stream(path, std::ios::binary | std::ios::ate);
		const auto length = stream.tellg();
		if (!stream || length <= 0 || length > 64 * 1024 * 1024)
		{
			throw std::runtime_error("Texture画像を開けないか、ファイルが空または64MiBを超えています: " + definition.Source());
		}
		std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
		stream.seekg(0);
		if (!stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
		{
			throw std::runtime_error("Texture画像を最後まで読み込めません: " + definition.Source());
		}
		auto extension = path.extension().wstring();
		for (auto& character : extension)
		{
			if (character >= L'A' && character <= L'Z')
			{
				character += L'a' - L'A';
			}
		}
		const bool dds = extension == L".dds";
		const bool tga = extension == L".tga";
		if (!dds && !tga && extension != L".png" && extension != L".jpg" && extension != L".jpeg" && extension != L".bmp")
		{
			throw std::invalid_argument("Texture画像はPNG/JPEG/BMP/TGA/DDSだけに対応しています。");
		}

		// 本体を展開する前に、配列・Cube・体積画像と過大な確保を拒否する。
		DirectX::TexMetadata metadata{};
		constexpr auto wicFlags = DirectX::WIC_FLAGS_IGNORE_SRGB;
		if (dds)
		{
			CheckImageResult(
				DirectX::GetMetadataFromDDSMemory(bytes.data(), bytes.size(), DirectX::DDS_FLAGS_NONE, metadata), "DDS metadata読込");
		}
		else if (tga)
		{
			CheckImageResult(
				DirectX::GetMetadataFromTGAMemory(bytes.data(), bytes.size(), DirectX::TGA_FLAGS_NONE, metadata), "TGA metadata読込");
		}
		else
		{
			CheckImageResult(DirectX::GetMetadataFromWICMemory(bytes.data(), bytes.size(), wicFlags, metadata), "WIC metadata読込");
		}
		if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || metadata.arraySize != 1 || metadata.depth != 1 ||
			metadata.IsCubemap() || metadata.width == 0 || metadata.height == 0 || metadata.width > 16384 || metadata.height > 16384 ||
			metadata.mipLevels == 0 || metadata.mipLevels > 15 || metadata.IsPMAlpha())
		{
			throw std::invalid_argument("Texture画像には16384以内の単一2D・straight alphaが必要です。");
		}
		// HDR・深度・動画formatはUnlitカラー素材の範囲外とする。
		const auto format = DirectX::MakeLinear(metadata.format);
		if (DirectX::IsTypeless(format) || DirectX::IsDepthStencil(format) || DirectX::IsPlanar(format) ||
			format == DXGI_FORMAT_BC6H_UF16 || format == DXGI_FORMAT_BC6H_SF16 || format == DXGI_FORMAT_R16_FLOAT ||
			format == DXGI_FORMAT_R16G16_FLOAT || format == DXGI_FORMAT_R16G16B16A16_FLOAT || format == DXGI_FORMAT_R32_FLOAT ||
			format == DXGI_FORMAT_R32G32_FLOAT || format == DXGI_FORMAT_R11G11B10_FLOAT || format == DXGI_FORMAT_R9G9B9E5_SHAREDEXP ||
			DirectX::BitsPerPixel(format) > 64)
		{
			throw std::invalid_argument("Texture画像のformatがRGBA8変換の対応範囲外です。");
		}
		std::size_t inputBytes = 0;
		std::size_t outputBytes = 0;
		auto width = metadata.width;
		auto height = metadata.height;
		for (std::size_t level = 0;; ++level)
		{
			std::size_t rowPitch = 0, slicePitch = 0;
			CheckImageResult(DirectX::ComputePitch(format, width, height, rowPitch, slicePitch), "展開寸法計算");
			inputBytes += level < metadata.mipLevels ? slicePitch : 0;
			if (definition.MipMode() == TextureMipMode::Generate || level < metadata.mipLevels)
			{
				outputBytes += width * height * 4;
			}
			if (inputBytes > TextureAsset::MaxDecodedBytes || outputBytes > TextureAsset::MaxDecodedBytes)
			{
				throw std::invalid_argument("Texture画像の展開後サイズが256MiBを超えています。");
			}
			if (width == 1 && height == 1)
			{
				if (metadata.mipLevels > level + 1)
				{
					throw std::invalid_argument("Texture画像のMip段数が寸法に対して過剰です。");
				}
				break;
			}
			width = (std::max)(std::size_t{1}, width / 2);
			height = (std::max)(std::size_t{1}, height / 2);
		}

		DirectX::ScratchImage decoded;
		if (dds)
		{
			CheckImageResult(
				DirectX::LoadFromDDSMemory(bytes.data(), bytes.size(), DirectX::DDS_FLAGS_NONE, nullptr, decoded), "DDS画素読込");
		}
		else if (tga)
		{
			CheckImageResult(
				DirectX::LoadFromTGAMemory(bytes.data(), bytes.size(), DirectX::TGA_FLAGS_NONE, nullptr, decoded), "TGA画素読込");
		}
		else
		{
			CheckImageResult(DirectX::LoadFromWICMemory(bytes.data(), bytes.size(), wicFlags, nullptr, decoded), "WIC画素読込");
		}
		// ファイルの色タグではなく素材定義を正本とし、ここでは数値を変えずにRGBAへ変換する。
		if (!decoded.OverrideFormat(format))
		{
			throw std::invalid_argument("Texture画像のformat解釈を統一できません。");
		}
		DirectX::ScratchImage converted;
		if (DirectX::IsCompressed(format))
		{
			CheckImageResult(DirectX::Decompress(decoded.GetImages(), decoded.GetImageCount(), decoded.GetMetadata(),
								 DXGI_FORMAT_R8G8B8A8_UNORM, converted),
				"BC展開");
		}
		else if (format != DXGI_FORMAT_R8G8B8A8_UNORM)
		{
			CheckImageResult(DirectX::Convert(decoded.GetImages(), decoded.GetImageCount(), decoded.GetMetadata(),
								 DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, 0.5f, converted),
				"RGBA8変換");
		}
		auto* pixels = converted.GetImageCount() ? &converted : &decoded;
		const auto targetFormat =
			definition.ColorSpace() == TextureColorSpace::Srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
		if (!pixels->OverrideFormat(targetFormat))
		{
			throw std::invalid_argument("Texture画像の色空間を設定できません。");
		}
		DirectX::ScratchImage generated;
		if (definition.MipMode() == TextureMipMode::Generate && (metadata.width > 1 || metadata.height > 1))
		{
			// 1x1は生成不要。sRGBは線形空間でフィルタし、保存画素をsRGBへ戻す。
			CheckImageResult(DirectX::GenerateMipMaps(*pixels->GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, generated), "Mip生成");
			pixels = &generated;
		}
		std::vector<TextureMip> mips;
		for (std::size_t level = 0; level < pixels->GetMetadata().mipLevels; ++level)
		{
			const auto& image = *pixels->GetImage(level, 0, 0);
			TextureMip mip{static_cast<std::uint32_t>(image.width), static_cast<std::uint32_t>(image.height), {}};
			mip.pixels.resize(image.width * image.height * 4);
			for (std::size_t row = 0; row < image.height; ++row)
			{
				std::memcpy(mip.pixels.data() + row * image.width * 4, image.pixels + row * image.rowPitch, image.width * 4);
			}
			mips.push_back(std::move(mip));
		}
		return TextureAsset(definition.Source(), definition.ColorSpace(), definition.MipMode(), std::move(mips));
	}
}
