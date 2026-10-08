#pragma once
#include <cstdint>
#include <limits>

namespace KT::World
{
	// World固有の非所有handle。形式と生存は別で、生存はWorld::IsAliveで調べる。
	struct Entity
	{
		static constexpr std::uint32_t InvalidIndex = (std::numeric_limits<std::uint32_t>::max)();
		std::uint64_t worldId = 0;
		std::uint32_t index = InvalidIndex;
		std::uint64_t generation = 0;

		[[nodiscard]] constexpr bool IsValid() const noexcept
		{
			return worldId != 0 && index != InvalidIndex && generation != 0;
		}
		bool operator==(const Entity&) const = default;
	};
}
