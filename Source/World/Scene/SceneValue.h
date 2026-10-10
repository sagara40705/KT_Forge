#pragma once
#include <cstdint>
#include <initializer_list>
#include <map>
#include <utility>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace KT::World
{
	// ファイル形式に依存しない所有設定値。64bit整数は10進文字列で保持する。
	struct SceneValue
	{
		// 要素の並びを保持する。Script配列はこの順序で復元する。
		using Array = std::vector<SceneValue>;
		// フィールド名で設定値を保持し、保存時のキー順を安定させる。
		using Object = std::map<std::string, SceneValue>;
		// 数値は有限のdouble。Componentの復元前に対象型の範囲を検査する。
		std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value{nullptr};

		SceneValue() = default;

		SceneValue(std::nullptr_t)
			: value(nullptr)
		{
		}

		SceneValue(bool input)
			: value(input)
		{
		}

		SceneValue(double input)
			: value(input)
		{
		}

		SceneValue(std::string input)
			: value(std::move(input))
		{
		}

		SceneValue(const char* input)
			: value(std::string(input))
		{
		}

		SceneValue(Array input)
			: value(std::move(input))
		{
		}

		SceneValue(Object input)
			: value(std::move(input))
		{
		}

		bool operator==(const SceneValue&) const = default;

		// 保存されている型を確認して取得する。別の型へ暗黙変換しない。
		[[nodiscard]] const Object& AsObject() const;
		[[nodiscard]] const Array& AsArray() const;
		[[nodiscard]] const std::string& AsString() const;
		[[nodiscard]] bool AsBool() const;
		[[nodiscard]] double AsNumber() const;
		// floatへの縮小で範囲外や非ゼロ値の消失が起きる場合は拒否する。
		[[nodiscard]] float AsFloat() const;
		// 1以上で小数部のない値だけを設定versionとして受け付ける。
		[[nodiscard]] std::uint32_t AsVersion() const;
		// 精度を保った10進文字列を解析し、対象整数型の範囲を検査する。
		[[nodiscard]] std::uint64_t AsUint64() const;
		[[nodiscard]] std::int64_t AsInt64() const;
		// 必須フィールドを取得する。欠落を既定値で補わない。
		[[nodiscard]] const SceneValue& At(std::string_view key) const;
		// 必須フィールドの欠落と、未定義フィールドを同時に拒否する。
		void RequireFields(std::initializer_list<std::string_view> fields) const;
		// 外部形式へ渡す前に、深さ・有限数値・UTF-8を検査する。
		void Validate(std::size_t depth = 0) const;
	};

	// 名前・設定キー・文字列値に使うUTF-8の妥当性を確認する。
	void ValidateSceneText(std::string_view text);
}
