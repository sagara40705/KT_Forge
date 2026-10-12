#include <Platform/FileReplacement.h>
#include <Core/Utility/NonCopyable.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace KT::Platform
{
	// この呼出しで新規作成した一時ファイルだけを所有し、途中失敗時に閉じて除去する。
	struct TemporaryFile : private KT::Core::NonCopyable
	{
		// 既存保存先とは異なる、この呼出し専用の名前と作成handle。
		std::filesystem::path path;
		HANDLE handle = INVALID_HANDLE_VALUE;
		// CREATE_NEW成功後だけ失敗時の除去を許可する。
		bool created = false;

		~TemporaryFile()
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

	void ReplaceFileWithText(const std::filesystem::path& path, std::string_view text, std::string_view temporaryId)
	{
		// 保存先と一時名を確認してから作成し、別ディレクトリや既存ファイルを掃除しない。
		if (path.empty() || path.filename().empty())
		{
			throw std::invalid_argument("ファイル保存先に有効なファイル名を指定してください。");
		}
		if (temporaryId.empty() || temporaryId.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-") !=
			std::string_view::npos)
		{
			throw std::invalid_argument("一時ファイルIDは空でない英数字とhyphenで指定してください。");
		}
		std::error_code error;
		const auto target = std::filesystem::absolute(path, error);
		if (error)
		{
			throw std::invalid_argument("ファイル保存先の絶対パスを取得できません。");
		}

		TemporaryFile temporary;
		temporary.path = target.parent_path() / (".kt-save-" + std::string(temporaryId) + ".tmp");
		temporary.handle = CreateFileW(temporary.path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (temporary.handle == INVALID_HANDLE_VALUE)
		{
			throw std::runtime_error("保存用一時ファイルのCreateFileWに失敗しました。Win32=" + std::to_string(GetLastError()));
		}
		temporary.created = true;

		// DWORDへ縮小できる範囲ごとに書き、全本文の完成後にflushする。
		std::size_t offset = 0;
		while (offset < text.size())
		{
			const auto size = static_cast<DWORD>((std::min)(text.size() - offset,
				static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
			DWORD written = 0;
			if (!WriteFile(temporary.handle, text.data() + offset, size, &written, nullptr))
			{
				throw std::runtime_error("保存用一時ファイルのWriteFileに失敗しました。Win32=" + std::to_string(GetLastError()));
			}
			if (written == 0)
			{
				throw std::runtime_error("保存用一時ファイルのWriteFileで書込が進みませんでした。");
			}
			offset += written;
		}
		if (!FlushFileBuffers(temporary.handle))
		{
			throw std::runtime_error("保存用一時ファイルのFlushFileBuffersに失敗しました。Win32=" + std::to_string(GetLastError()));
		}
		const auto closed = CloseHandle(temporary.handle);
		temporary.handle = INVALID_HANDLE_VALUE;
		if (!closed)
		{
			throw std::runtime_error("保存用一時ファイルのCloseHandleに失敗しました。Win32=" + std::to_string(GetLastError()));
		}

		// 完成ファイルで置換し、既存保存先を先に削除しない。
		if (!MoveFileExW(temporary.path.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			throw std::runtime_error("保存先のMoveFileExWによる置換に失敗しました。Win32=" + std::to_string(GetLastError()));
		}
		temporary.created = false;
	}
}
