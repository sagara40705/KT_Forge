#pragma once
#include <Application/FixedStepClock.h>
#include <Core/Utility/NonCopyable.h>

namespace KT::Application
{
	// Sceneの時間進行だけを制御する。Window/UIの更新や終了要求とは分ける。
	enum class ExecutionState
	{
		Playing,
		Paused,
		Stopped
	};

	// 一回の外側loopで実行する固定tick。一コマ更新はポーズ状態を保つ。
	struct SimulationAdvance
	{
		unsigned ticks = 0;
		bool singleStep = false;
	};

	// 単一threadで再生状態と固定時計を所有し、停止時間を追いつき更新へ持ち越さない。
	class ExecutionControl : private KT::Core::NonCopyable
	{
	public:
		void Play() noexcept;
		void Pause() noexcept;
		void Stop() noexcept;
		// ポーズ中だけ一回を予約する。消費前の複数要求は一回へまとめる。
		void RequestSingleStep();
		// 呼出側のTimer再設定と同時に時計端数を捨てる。一コマ予約は保持する。
		void ResetClock() noexcept;
		[[nodiscard]] SimulationAdvance Advance(double elapsedSeconds);
		[[nodiscard]] ExecutionState State() const noexcept;
		[[nodiscard]] double DroppedSeconds() const noexcept;

	private:
		// 従来のRunと同じく、指定がなければ起動から再生する。
		ExecutionState state_ = ExecutionState::Playing;
		FixedStepClock clock_;
		// 状態遷移した回の経過時間は旧状態の時間なので受け入れない。
		bool resetClock_ = false;
		bool singleStepRequested_ = false;
	};
}
