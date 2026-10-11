#include <Content/AssetId.h>
#include <algorithm>
#include <random>
#include <stdexcept>

namespace KT::Content
{
	bool AssetId::IsValid() const noexcept
	{
		return std::any_of(bytes.begin(), bytes.end(), [](auto byte) { return byte != 0; });
	}

	std::size_t AssetIdHash::operator()(const AssetId& id) const noexcept
	{
		std::uint64_t hash = 14695981039346656037ULL;
		for (const auto byte : id.bytes)
		{
			hash = (hash ^ byte) * 1099511628211ULL;
		}
		return static_cast<std::size_t>(hash);
	}

	AssetId AssetId::Generate()
	{
		// 永続化できる乱数値を作り、UUIDのversionとvariantを設定する。
		std::random_device random;
		std::uniform_int_distribution<unsigned int> distribution(0, 255);
		AssetId uuid;
		for (auto& byte : uuid.bytes)
		{
			byte = static_cast<std::uint8_t>(distribution(random));
		}
		uuid.bytes[6] = static_cast<std::uint8_t>((uuid.bytes[6] & 0x0f) | 0x40);
		uuid.bytes[8] = static_cast<std::uint8_t>((uuid.bytes[8] & 0x3f) | 0x80);
		return uuid;
	}

	AssetId AssetId::Parse(std::string_view text)
	{
		if (text.size() != 36)
		{
			throw std::invalid_argument("AssetIdは8-4-4-4-12形式の36文字で指定してください。");
		}

		// 区切り位置と16進数を検査し、大小文字を同じ16byte値へ戻す。
		AssetId uuid;
		std::size_t byteIndex = 0;
		unsigned int current = 0;
		std::size_t digits = 0;
		for (std::size_t index = 0; index < text.size(); ++index)
		{
			if (index == 8 || index == 13 || index == 18 || index == 23)
			{
				if (text[index] != '-')
				{
					throw std::invalid_argument("AssetIdの区切り位置が不正です。");
				}
				continue;
			}
			const auto character = text[index];
			unsigned int digit = 0;
			if (character >= '0' && character <= '9')
			{
				digit = static_cast<unsigned int>(character - '0');
			}
			else if (character >= 'a' && character <= 'f')
			{
				digit = static_cast<unsigned int>(character - 'a' + 10);
			}
			else if (character >= 'A' && character <= 'F')
			{
				digit = static_cast<unsigned int>(character - 'A' + 10);
			}
			else
			{
				throw std::invalid_argument("AssetIdに16進数以外の文字があります。");
			}
			current = current * 16 + digit;
			if (++digits % 2 == 0)
			{
				uuid.bytes[byteIndex++] = static_cast<std::uint8_t>(current);
				current = 0;
			}
		}
		if (!uuid.IsValid())
		{
			throw std::invalid_argument("AssetIdの全ゼロ値は保存用IDとして使えません。");
		}
		return uuid;
	}

	std::string AssetId::ToString() const
	{
		if (!IsValid())
		{
			throw std::invalid_argument("無効なAssetIdを文字列へ変換できません。");
		}
		constexpr char digits[] = "0123456789abcdef";
		std::string text;
		text.reserve(36);
		for (std::size_t index = 0; index < bytes.size(); ++index)
		{
			if (index == 4 || index == 6 || index == 8 || index == 10)
			{
				text.push_back('-');
			}
			text.push_back(digits[bytes[index] >> 4]);
			text.push_back(digits[bytes[index] & 15]);
		}
		return text;
	}
}
