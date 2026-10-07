#pragma once
#include <chrono>

namespace KT::Core
{
	// Monotonic seconds. Owns no external resource; copying copies the time snapshot.
	// One instance requires external synchronization if shared between threads.
	class Timer
	{
	public:
		Timer() noexcept { Reset(); }

		// Restarts elapsed time and the Tick baseline; stored delta becomes zero.
		void Reset() noexcept
		{
			start_ = lastTick_ = Clock::now();
			deltaSeconds_ = 0.0;
		}

		// Samples now once. Returns/stores seconds since Reset or the previous Tick.
		[[nodiscard]] double Tick() noexcept
		{
			const auto now = Clock::now();
			deltaSeconds_ = std::chrono::duration<double>(now - lastTick_).count();
			lastTick_ = now;
			return deltaSeconds_;
		}

		// Live time since Reset; querying does not advance the Tick baseline.
		[[nodiscard]] double GetElapsedSeconds() const noexcept
		{
			return std::chrono::duration<double>(Clock::now() - start_).count();
		}

		[[nodiscard]] double GetDeltaSeconds() const noexcept { return deltaSeconds_; }

	private:
		using Clock = std::chrono::steady_clock;
		Clock::time_point start_;
		Clock::time_point lastTick_;
		double deltaSeconds_ = 0.0;
	};
}
