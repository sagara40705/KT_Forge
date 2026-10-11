#pragma once
#include <Content/AssetDocument.h>
#include <filesystem>

namespace KT::Content
{
	// Mesh/Material/Texture/Modelの保存定義をCPU原本へ変換する。JSON型を公開契約へ出さない。
	class AssetJson
	{
	public:
		static constexpr std::size_t MaxBytes = 64 * 1024 * 1024;
		static constexpr int MaxDepth = 16;
		[[nodiscard]] static AssetDocument Parse(std::string_view text);
		[[nodiscard]] static std::string Stringify(const AssetDocument& document);
		[[nodiscard]] static AssetDocument Read(const std::filesystem::path& path);
		// 一時ファイルへの書込成功後に保存先を置換する。失敗時は既存ファイルを保持する。
		static void Save(const std::filesystem::path& path, const AssetDocument& document);
	};
}
