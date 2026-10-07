#pragma once
#include <source_location>
#include <string_view>

namespace KT::Core
{
	enum class LogLevel
	{
		Info,
		Warning,
		Error,
	};

	// Synchronously writes message, level and caller location to stderr, then flushes.
	// Calls to Log are serialized; other direct stderr writers are not covered.
	// Returns false on invalid level, stream/locking failure or exception; never throws.
	// A failed write may leave partial output and stderr in a failed state. No retry.
	// Message is borrowed only during this call; bytes/embedded newlines are unchanged.
	[[nodiscard]] bool Log(LogLevel level, std::string_view message,
		std::source_location location = std::source_location::current()) noexcept;
}
