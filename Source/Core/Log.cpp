#include "Log.h"
#include <cstdint>
#include <iostream>
#include <limits>
#include <mutex>

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
