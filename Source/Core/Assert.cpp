#include "Assert.h"
#include "Log.h"
#include <cstdlib>

[[noreturn]] void KT::Core::Detail::AssertFailure(std::string_view expression,
	std::source_location location) noexcept
{
	(void)Log(LogLevel::Error, "Assertion failed:", location);
	(void)Log(LogLevel::Error, expression, location);
	std::abort();
}
