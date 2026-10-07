#include "File.h"
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace
{
	[[noreturn]] void ReadFailure(const std::filesystem::path& path, std::string_view cause)
	{
		const auto utf8 = path.u8string();
		const std::string display(utf8.begin(), utf8.end());
		throw std::runtime_error("File read '" + display + "': " + std::string(cause));
	}

	template<class Buffer>
	Buffer Read(const std::filesystem::path& path)
	{
		if (path.empty() || path.native().find(std::filesystem::path::value_type{}) != std::filesystem::path::string_type::npos)
		{
			ReadFailure(path, "empty path or embedded NUL");
		}
		std::error_code error;
		const auto status = std::filesystem::status(path, error);
		if (error) ReadFailure(path, error.message());
		if (!std::filesystem::is_regular_file(status)) ReadFailure(path, "not a regular file");

		// Textも改行などを変換せず読むためbinaryを使い、最初に末尾位置からサイズを調べる。
		std::ifstream stream(path, std::ios::binary | std::ios::ate);
		if (!stream) ReadFailure(path, "cannot open file for reading");
		const std::streamoff end = stream.tellg();
		if (end < 0) ReadFailure(path, "cannot determine file size");
		const auto size = static_cast<std::uintmax_t>(end);
		Buffer result;
		// 確保可能な要素数と1回のreadで扱えるサイズを確認してから、各型へ変換する。
		if (size > result.max_size() ||
			size > static_cast<std::uintmax_t>((std::numeric_limits<std::streamsize>::max)()))
		{
			ReadFailure(path, "file size exceeds buffer or stream limit");
		}
		try
		{
			result.resize(static_cast<typename Buffer::size_type>(size));
		}
		catch (const std::exception& exception)
		{
			ReadFailure(path, exception.what());
		}
		stream.seekg(0, std::ios::beg);
		if (!stream) ReadFailure(path, "cannot seek to start");
		if (!result.empty())
		{
			const auto count = static_cast<std::streamsize>(size);
			stream.read(reinterpret_cast<char*>(result.data()), count);
			if (!stream || stream.gcount() != count) ReadFailure(path, "read failed or file became shorter");
		}
		// 調べたサイズの後にデータがあれば、読込中の増大として失敗させる。
		// 同じサイズでの書換えまでは検出できないので、内容の一貫性を保証する検査ではない。
		if (stream.peek() != std::ifstream::traits_type::eof()) ReadFailure(path, "file grew while reading");
		if (stream.bad() || !stream.eof()) ReadFailure(path, "cannot verify end of file");
		return result;
	}
}

std::vector<std::byte> KT::Core::File::ReadBinary(const std::filesystem::path& path)
{
	return Read<std::vector<std::byte>>(path);
}

std::string KT::Core::File::ReadText(const std::filesystem::path& path)
{
	return Read<std::string>(path);
}
