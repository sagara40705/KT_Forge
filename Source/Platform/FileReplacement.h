#pragma once
#include <filesystem>
#include <string_view>

namespace KT::Platform
{
	// 検証済みの本文を同じディレクトリの一時ファイルへ書き、flush・close成功後だけ置換する。
	// temporaryIdは呼出側が発行した一意な英数字/hyphen。作成に成功した一時ファイルだけ後片付けする。
	// 形式の検証・サイズ制限・複数ファイルの一括保存は呼出側の責務。
	void ReplaceFileWithText(const std::filesystem::path& path, std::string_view text, std::string_view temporaryId);
}
