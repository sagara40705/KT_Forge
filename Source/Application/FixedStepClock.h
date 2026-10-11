#pragma once
#include <Physics/PhysicsTypes.h>

namespace KT::Application
{
	// 描画の実時間を固定tick数へ変換する。過負荷時の無制限な追いつきを防ぐ。
	class FixedStepClock
	{
	public:
		static constexpr unsigned MaxTicksPerFrame = 4;
		unsigned Advance(double elapsedSeconds);

		// 再開・Resizeの待機時間を次のsimulationへ持ち越さない。
		void Reset() noexcept
		{
			accumulator_ = 0;
		}

		[[nodiscard]] double DroppedSeconds() const noexcept
		{
			return droppedSeconds_;
		}

	private:
		double accumulator_ = 0;
		double droppedSeconds_ = 0;
	};
}
