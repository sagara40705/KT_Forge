#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace KT::World
{
	// 保存用の識別値。実行中のEntity handleとは独立している。
	struct ObjectUuid
	{
		// 全バイト0を無効値とする、16バイトの保存用識別値。
		std::array<std::uint8_t, 16> bytes{};
		[[nodiscard]] bool IsValid() const noexcept;
		[[nodiscard]] static ObjectUuid Generate();
		[[nodiscard]] static ObjectUuid Parse(std::string_view text);
		[[nodiscard]] std::string ToString() const;
		auto operator<=>(const ObjectUuid&) const = default;
	};

	// Scene読込をまたいで同じオブジェクトを識別するcomponent。
	struct PersistentId
	{
		ObjectUuid value;
	};

	// Editorで表示・編集するオブジェクト名を保持するcomponent。
	struct Name
	{
		std::string value;
	};
}
