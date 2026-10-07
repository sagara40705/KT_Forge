#pragma once
#include <source_location>
#include <string_view>

namespace KT::Core::Detail
{
	// Developer invariant failure: best-effort diagnostic, then abort (no exception).
	[[noreturn]] void AssertFailure(std::string_view expression,
		std::source_location location) noexcept;
}

// Internal invariants only. Required input/runtime checks must use normal code.
// Debug: condition is evaluated once. NDEBUG: it is not evaluated at all.
// Never put required side effects (initialization/API calls) inside KT_ASSERT.
#ifndef NDEBUG
#define KT_ASSERT(condition) do { \
	if (!static_cast<bool>(condition)) { \
		::KT::Core::Detail::AssertFailure(#condition, ::std::source_location::current()); \
	} \
} while (false)
#else
#define KT_ASSERT(condition) do { } while (false)
#endif
