#include <Content/AssetJson.h>
#include <Platform/FileReplacement.h>
#include <fstream>

namespace KT::Content
{
	AssetDocument AssetJson::Read(const std::filesystem::path& path)
	{
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file)
		{
			throw std::runtime_error("素材JSONファイルを開けません。");
		}
		const auto size = file.tellg();
		if (size < 0 || size > static_cast<std::streamoff>(MaxBytes))
		{
			throw std::runtime_error("素材JSONファイルのサイズが不正、または64MiBを超えています。");
		}
		std::string text(static_cast<std::size_t>(size), '\0');
		file.seekg(0);
		file.read(text.data(), static_cast<std::streamsize>(text.size()));
		if (!file || file.peek() != std::char_traits<char>::eof())
		{
			throw std::runtime_error("素材JSONの読込に失敗したか、読込中にサイズが変わりました。");
		}
		return Parse(text);
	}
	void AssetJson::Save(const std::filesystem::path& path, const AssetDocument& document)
	{
		// 出力全体を完成させてから一時ファイルを作り、既存の保存先に触れない。
		const auto text = Stringify(document);
		KT::Platform::ReplaceFileWithText(path, text, AssetId::Generate().ToString());
	}
}
