#include <Application/FixedStepClock.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace KT::Application
{
	unsigned FixedStepClock::Advance(double elapsedSeconds)
	{
		if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0)
		{
			throw std::invalid_argument("固定tickの経過時間には0以上の有限値が必要です。");
		}
		const double accepted = (std::min)(elapsedSeconds, 0.25);
		droppedSeconds_ += elapsedSeconds - accepted;
		accumulator_ += accepted;
		const auto available = static_cast<unsigned>(std::floor((accumulator_ + 1.0e-12) / KT::Physics::FixedDeltaSeconds));
		const auto ticks = (std::min)(available, MaxTicksPerFrame);
		accumulator_ = (std::max)(0.0, accumulator_ - available * KT::Physics::FixedDeltaSeconds);
		droppedSeconds_ += (available - ticks) * KT::Physics::FixedDeltaSeconds;
		return ticks;
	}
}
