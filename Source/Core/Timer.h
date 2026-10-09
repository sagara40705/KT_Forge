#pragma once
#include <chrono>

namespace KT::Core
{
	// システム時刻の変更に影響されないsteady_clockで、時間を秒単位のdoubleとして測る。
	// コピーすると計測開始・直前のTickの時刻と保存済みの差分が引き継がれ、以後は別々に更新できる。
	// 同じTimerを複数スレッドで共有する場合、呼出側で同時アクセスを防ぐ。
	class Timer
	{
	public:
		Timer() noexcept
		{
			Reset();
		}

		// ここを新しい計測開始点とする。次のTickの基準もここに戻し、保存済みの差分を0にする。
		void Reset() noexcept
		{
			start_ = lastTick_ = Clock::now();
			deltaSeconds_ = 0.0;
		}

		// 前回のTickからの時間を返し、同じ値を保存する。初回は生成またはResetからの時間。
		// 現在時刻は1回だけ取得し、今回の計測終点を次回の計測開始点にする。
		[[nodiscard]] double Tick() noexcept
		{
			const auto now = Clock::now();
			deltaSeconds_ = std::chrono::duration<double>(now - lastTick_).count();
			lastTick_ = now;
			return deltaSeconds_;
		}

		// 生成またはResetから、この呼出時点までの経過時間を返す。
		// Tickの基準は動かさないため、この値を確認しても次のTickの差分には影響しない。
		[[nodiscard]] double GetElapsedSeconds() const noexcept
		{
			return std::chrono::duration<double>(Clock::now() - start_).count();
		}

		// 直前のTickで保存した差分を返す。ここでは時刻を測り直さない。生成直後とReset直後は0。
		[[nodiscard]] double GetDeltaSeconds() const noexcept
		{
			return deltaSeconds_;
		}

	private:
		using Clock = std::chrono::steady_clock;
		Clock::time_point start_;
		Clock::time_point lastTick_;
		double deltaSeconds_ = 0.0;
	};
}
