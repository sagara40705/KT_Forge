#include <World/Scene/SceneValue.h>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace KT::World
{
	void ValidateSceneText(std::string_view text)
	{
		// UTF-8の過長表現、サロゲート、範囲外と途中切れを拒否する。
		for (std::size_t index = 0; index < text.size();)
		{
			const auto first = static_cast<unsigned char>(text[index++]);
			if (first < 0x80)
			{
				continue;
			}
			std::size_t remaining = 0;
			std::uint32_t code = 0;
			std::uint32_t minimum = 0;
			if (first >= 0xc2 && first <= 0xdf)
			{
				remaining = 1;
				code = first & 0x1f;
				minimum = 0x80;
			}
			else if (first >= 0xe0 && first <= 0xef)
			{
				remaining = 2;
				code = first & 0x0f;
				minimum = 0x800;
			}
			else if (first >= 0xf0 && first <= 0xf4)
			{
				remaining = 3;
				code = first & 0x07;
				minimum = 0x10000;
			}
			else
			{
				throw std::invalid_argument("Scene文字列のUTF-8先頭byteが不正です。");
			}
			if (remaining > text.size() - index)
			{
				throw std::invalid_argument("Scene文字列のUTF-8が途中で切れています。");
			}
			for (std::size_t offset = 0; offset < remaining; ++offset)
			{
				const auto next = static_cast<unsigned char>(text[index++]);
				if ((next & 0xc0) != 0x80)
				{
					throw std::invalid_argument("Scene文字列のUTF-8継続byteが不正です。");
				}
				code = code * 64 + (next & 0x3f);
			}
			if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
			{
				throw std::invalid_argument("Scene文字列のUTF-8コードポイントが不正です。");
			}
		}
	}

	void SceneValue::Validate(std::size_t depth) const
	{
		if (depth > 128 || value.valueless_by_exception())
		{
			throw std::invalid_argument("Scene設定値の深さが128を超えたか、値の型が失効しています。");
		}
		if (const auto* number = std::get_if<double>(&value))
		{
			if (!std::isfinite(*number))
			{
				throw std::invalid_argument("Scene設定値に非有限の数値があります。");
			}
		}
		else if (const auto* text = std::get_if<std::string>(&value))
		{
			ValidateSceneText(*text);
		}
		else if (const auto* array = std::get_if<Array>(&value))
		{
			for (const auto& element : *array)
			{
				element.Validate(depth + 1);
			}
		}
		else if (const auto* object = std::get_if<Object>(&value))
		{
			for (const auto& [key, element] : *object)
			{
				ValidateSceneText(key);
				element.Validate(depth + 1);
			}
		}
	}

	const SceneValue::Object& SceneValue::AsObject() const
	{
		const auto* result = std::get_if<Object>(&value);
		if (!result)
		{
			throw std::invalid_argument("Scene設定値はobjectである必要があります。");
		}
		return *result;
	}

	const SceneValue::Array& SceneValue::AsArray() const
	{
		const auto* result = std::get_if<Array>(&value);
		if (!result)
		{
			throw std::invalid_argument("Scene設定値はarrayである必要があります。");
		}
		return *result;
	}

	const std::string& SceneValue::AsString() const
	{
		const auto* result = std::get_if<std::string>(&value);
		if (!result)
		{
			throw std::invalid_argument("Scene設定値はstringである必要があります。");
		}
		return *result;
	}

	bool SceneValue::AsBool() const
	{
		const auto* result = std::get_if<bool>(&value);
		if (!result)
		{
			throw std::invalid_argument("Scene設定値はboolである必要があります。");
		}
		return *result;
	}

	double SceneValue::AsNumber() const
	{
		const auto* result = std::get_if<double>(&value);
		if (!result || !std::isfinite(*result))
		{
			throw std::invalid_argument("Scene設定値は有限の数値である必要があります。");
		}
		return *result;
	}

	float SceneValue::AsFloat() const
	{
		const auto number = AsNumber();
		if (number < -(std::numeric_limits<float>::max)() || number > (std::numeric_limits<float>::max)())
		{
			throw std::invalid_argument("Scene設定値がfloatの範囲を超えています。");
		}
		const auto result = static_cast<float>(number);
		if (number != 0 && result == 0)
		{
			throw std::invalid_argument("Scene設定値がfloatの表現範囲より小さいため、0へ縮小できません。");
		}
		return result;
	}

	std::uint32_t SceneValue::AsVersion() const
	{
		const auto number = AsNumber();
		if (number < 1 || number > (std::numeric_limits<std::uint32_t>::max)() || std::trunc(number) != number)
		{
			throw std::invalid_argument("Scene設定versionは1以上のuint32整数である必要があります。");
		}
		return static_cast<std::uint32_t>(number);
	}

	std::uint64_t SceneValue::AsUint64() const
	{
		const auto& text = AsString();
		std::uint64_t result = 0;
		const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
		if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || std::to_string(result) != text)
		{
			throw std::invalid_argument("Sceneのuint64設定は範囲内の正規10進文字列である必要があります。");
		}
		return result;
	}

	std::int64_t SceneValue::AsInt64() const
	{
		const auto& text = AsString();
		std::int64_t result = 0;
		const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
		if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || std::to_string(result) != text)
		{
			throw std::invalid_argument("Sceneのint64設定は範囲内の正規10進文字列である必要があります。");
		}
		return result;
	}

	const SceneValue& SceneValue::At(std::string_view key) const
	{
		const auto& object = AsObject();
		const auto found = object.find(std::string(key));
		if (found == object.end())
		{
			throw std::invalid_argument("Scene設定の必須フィールドがありません: " + std::string(key));
		}
		return found->second;
	}

	void SceneValue::RequireFields(std::initializer_list<std::string_view> fields) const
	{
		const auto& object = AsObject();
		for (const auto field : fields)
		{
			(void)At(field);
		}
		if (object.size() != fields.size())
		{
			throw std::invalid_argument("Scene設定に未定義のフィールドがあります。");
		}
	}
}
