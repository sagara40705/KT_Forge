#include <Application/ExecutionControl.h>
#include <cmath>
#include <stdexcept>

namespace KT::Application
{
	void ExecutionControl::Play() noexcept
	{
		if (state_ != ExecutionState::Playing)
		{
			state_ = ExecutionState::Playing;
			ResetClock();
			resetClock_ = true;
		}
		singleStepRequested_ = false;
	}

	void ExecutionControl::Pause() noexcept
	{
		if (state_ != ExecutionState::Paused)
		{
			state_ = ExecutionState::Paused;
			ResetClock();
			resetClock_ = true;
		}
	}

	void ExecutionControl::Stop() noexcept
	{
		state_ = ExecutionState::Stopped;
		singleStepRequested_ = false;
		ResetClock();
		resetClock_ = true;
	}

	void ExecutionControl::RequestSingleStep()
	{
		if (state_ != ExecutionState::Paused)
		{
			throw std::logic_error("一コマ更新はポーズ中だけ予約できます。");
		}
		singleStepRequested_ = true;
	}

	void ExecutionControl::ResetClock() noexcept
	{
		clock_.Reset();
		// 外側のTimerも再設定済みなので、次の新しい経過時間は受け入れる。
		resetClock_ = false;
	}

	SimulationAdvance ExecutionControl::Advance(double elapsedSeconds)
	{
		// 不正な時間で予約を消費したり、状態を変更したりしない。
		if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0)
		{
			throw std::invalid_argument("実行制御の経過時間には0以上の有限値が必要です。");
		}

		if (resetClock_)
		{
			elapsedSeconds = 0;
			resetClock_ = false;
		}
		if (state_ == ExecutionState::Playing)
		{
			return {clock_.Advance(elapsedSeconds), false};
		}

		// 停止時間は負荷による破棄時間にも計上せず、一コマだけ固定deltaで進める。
		clock_.Reset();
		if (state_ == ExecutionState::Paused && singleStepRequested_)
		{
			singleStepRequested_ = false;
			return {1, true};
		}
		return {};
	}

	ExecutionState ExecutionControl::State() const noexcept
	{
		return state_;
	}

	double ExecutionControl::DroppedSeconds() const noexcept
	{
		return clock_.DroppedSeconds();
	}
}
