#pragma once
#include <string>

namespace KT::World
{
	// 個体ごとの再生値。共有素材・ozz型・GPU行列を持たず、Scene transactionで復元する。
	struct Animator
	{
		std::string clip;
		double seconds = 0;
		double speed = 1;
		bool loop = true;
		bool playing = true;
		bool enabled = true;
		// 遷移元は現在Clip、遷移先だけを別時刻で進める。
		std::string nextClip;
		double nextSeconds = 0;
		bool nextLoop = true;
		double blendDuration = 0;
		double blendElapsed = 0;

		void Validate() const;
		void Play(std::string name, bool repeat = true, double playbackSpeed = 1, double startSeconds = 0);
		void CrossFade(std::string name, double duration, bool repeat = true, double startSeconds = 0);
		void Seek(double time);
		void Advance(double deltaSeconds);
		[[nodiscard]] bool IsFinished(double duration) const;

		// 所有文字列も確保せず交換し、更新失敗時に元の再生値へ戻す。
		friend void swap(Animator& first, Animator& second) noexcept;
	};
}
