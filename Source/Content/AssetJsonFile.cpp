#include <Content/AssetJson.h>
#include <Core/Utility/NonCopyable.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
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

	// 自分が作成した一時ファイルだけを、保存失敗時に閉じて削除する。
	struct TemporaryAssetFile : private KT::Core::NonCopyable
	{
		std::filesystem::path path;
		HANDLE handle = INVALID_HANDLE_VALUE;
		bool created = false;

		~TemporaryAssetFile()
		{
			if (handle != INVALID_HANDLE_VALUE)
			{
				CloseHandle(handle);
			}
			if (created)
			{
				DeleteFileW(path.c_str());
			}
		}
	};

	void AssetJson::Save(const std::filesystem::path& path, const AssetDocument& document)
	{
		// 出力全体を完成させてから一時ファイルを作り、既存の保存先に触れない。
		const auto text = Stringify(document);
		TemporaryAssetFile temporary;
		temporary.path = path;
		temporary.path += "." + AssetId::Generate().ToString() + ".tmp";
		temporary.handle = CreateFileW(temporary.path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (temporary.handle == INVALID_HANDLE_VALUE)
		{
			throw std::runtime_error("素材JSONの一時ファイルを作成できません。Win32=" + std::to_string(GetLastError()));
		}
		temporary.created = true;
		DWORD written = 0;
		if (!WriteFile(temporary.handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) || written != text.size())
		{
			throw std::runtime_error("素材JSONの一時ファイルへの書込に失敗しました。");
		}
		if (!FlushFileBuffers(temporary.handle))
		{
			throw std::runtime_error("素材JSONの一時ファイルのflushに失敗しました。");
		}
		const auto handle = temporary.handle;
		temporary.handle = INVALID_HANDLE_VALUE;
		if (!CloseHandle(handle))
		{
			throw std::runtime_error("素材JSONの一時ファイルを閉じられません。");
		}
		if (!MoveFileExW(temporary.path.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			throw std::runtime_error("素材JSONの保存先置換に失敗しました。Win32=" + std::to_string(GetLastError()));
		}
		temporary.created = false;
	}
}
