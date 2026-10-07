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

	// Synchronously writes message, level and caller location to stderr.
	// Pass Japanese messages as UTF-8 (ordinary literals with the project's /utf-8).
	// Windows console: UTF-8 is decoded and written as Unicode without changing its
	// code page; invalid UTF-8 returns false. Redirected stderr: bytes unchanged + flush.
	// Console output uses the OS stderr handle; custom cerr buffers apply to redirection.
	// Calls to Log are serialized; other direct stderr writers are not covered.
	// Returns false on invalid level, stream/locking failure or exception; never throws.
	// A failed write may leave partial output and stderr in a failed state. No retry.
	// Message is borrowed only during this call. Embedded newlines are preserved.
	[[nodiscard]] bool Log(LogLevel level, std::string_view message,
		std::source_location location = std::source_location::current()) noexcept;
}

// Convenience macros: evaluate the message once and intentionally discard output status.
// Caller location is preserved; logging remains enabled in Release.
#define KT_LOG_INFO(message) \
	((void)::KT::Core::Log(::KT::Core::LogLevel::Info, (message)))

#define KT_LOG_WARNING(message) \
	((void)::KT::Core::Log(::KT::Core::LogLevel::Warning, (message)))

#define KT_LOG_ERROR(message) \
	((void)::KT::Core::Log(::KT::Core::LogLevel::Error, (message)))
