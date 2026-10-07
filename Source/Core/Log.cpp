#include "Log.h"
#include <cstdint>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

bool KT::Core::Log(LogLevel level, std::string_view message,
	std::source_location location) noexcept
{
	const char* label = nullptr;
	switch (level)
	{
	case LogLevel::Info: label = "Info"; break;
	case LogLevel::Warning: label = "Warning"; break;
	case LogLevel::Error: label = "Error"; break;
	default: return false;
	}
	if (message.size() > static_cast<std::uintmax_t>((std::numeric_limits<std::streamsize>::max)()))
	{
		return false;
	}

	try
	{
		static std::mutex outputMutex;
		const std::lock_guard lock(outputMutex);
		if (!std::cerr) return false;
		const HANDLE output = GetStdHandle(STD_ERROR_HANDLE);
		DWORD consoleMode = 0;
		if (GetConsoleMode(output, &consoleMode))
		{
			// 日本語の表示がコンソールのコードページに左右されないよう、
			// ログ全体をUTF-8からUTF-16へ変換してWriteConsoleWへ渡す。OS設定は変えない。
			std::ostringstream record;
			record << '[' << label << "] " << location.file_name() << ':'
				<< location.line() << " (" << location.function_name() << ") ";
			if (!message.empty()) record.write(message.data(), static_cast<std::streamsize>(message.size()));
			record << '\n';
			if (!record) return false;
			const auto utf8 = record.str();
			if (utf8.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) return false;
			const auto byteCount = static_cast<int>(utf8.size());
			const int wideCount = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
				utf8.data(), byteCount, nullptr, 0);
			if (wideCount == 0) return false;
			std::wstring wide(static_cast<std::size_t>(wideCount), L'\0');
			if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), byteCount,
				wide.data(), wideCount) != wideCount) return false;
			DWORD written = 0;
			return WriteConsoleW(output, wide.data(), static_cast<DWORD>(wideCount), &written, nullptr)
				&& written == static_cast<DWORD>(wideCount);
		}
		std::cerr << '[' << label << "] " << location.file_name() << ':'
			<< location.line() << " (" << location.function_name() << ") ";
		if (!message.empty())
		{
			std::cerr.write(message.data(), static_cast<std::streamsize>(message.size()));
		}
		std::cerr << '\n';
		std::cerr.flush();
		return static_cast<bool>(std::cerr);
	}
	catch (...)
	{
		return false;
	}
}
