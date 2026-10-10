#include <World/Scene/ObjectIdentity.h>
#include <algorithm>
#include <random>

namespace KT::World
{
	bool ObjectUuid::IsValid() const noexcept
	{
		return std::any_of(bytes.begin(), bytes.end(), [](auto byte) { return byte != 0; });
	}

	ObjectUuid ObjectUuid::Generate()
	{
		// 永続化できる乱数値を作り、UUIDのversionとvariantを設定する。
		std::random_device random;
		std::uniform_int_distribution<unsigned int> distribution(0, 255);
		ObjectUuid uuid;
		for (auto& byte : uuid.bytes)
		{
			byte = static_cast<std::uint8_t>(distribution(random));
		}
		uuid.bytes[6] = static_cast<std::uint8_t>((uuid.bytes[6] & 0x0f) | 0x40);
		uuid.bytes[8] = static_cast<std::uint8_t>((uuid.bytes[8] & 0x3f) | 0x80);
		return uuid;
	}
}
