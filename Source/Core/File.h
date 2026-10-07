#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace KT::Core::File
{
	// Reads one regular, seekable file into an owning value; an empty file succeeds.
	// Native filesystem paths preserve Windows Unicode. Relative paths use the CWD.
	// Ordinary read/path/size failures throw runtime_error with UTF-8 path and cause.
	// Allocation failures are annotated where possible; if diagnostics cannot be
	// allocated, standard allocation exceptions propagate. Paths must be valid Unicode.
	// No partial value is returned. Concurrent file mutation is unsupported: shrink
	// and detected growth fail, but this is not an atomic snapshot or file lock.
	[[nodiscard]] std::vector<std::byte> ReadBinary(const std::filesystem::path& path);

	// Same bytes in a string: preserves BOM, CRLF and NUL; no encoding validation,
	// conversion or terminator from the file is added. Caller interprets the encoding.
	[[nodiscard]] std::string ReadText(const std::filesystem::path& path);
}
