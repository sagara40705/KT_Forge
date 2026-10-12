#include <World/Animation/Animator.h>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace KT::World
{
	void Animator::Validate() const
	{
		if (clip.size() > 256 || nextClip.size() > 256 || clip.find('\0') != std::string::npos || nextClip.find('\0') != std::string::npos)
		{
			throw std::invalid_argument("AnimatorのClip名にはNULを含まない256byte以下の文字列が必要です。");
		}
		if (!std::isfinite(seconds) || !std::isfinite(nextSeconds) || !std::isfinite(speed) || !std::isfinite(blendDuration) ||
			!std::isfinite(blendElapsed) || blendDuration < 0 || blendElapsed < 0)
		{
			throw std::invalid_argument("Animatorの再生時刻・速度・遷移秒数が不正です。");
		}
		if ((blendDuration > 0 && nextClip.empty()) || blendElapsed > blendDuration)
		{
			throw std::invalid_argument("Animatorの遷移先Clipまたは遷移進行値が不正です。");
		}
	}

	void Animator::Play(std::string name, bool repeat, double playbackSpeed, double startSeconds)
	{
		// 完成した候補だけを交換し、不正な要求では元の再生を保持する。
		Animator candidate = *this;
		candidate.clip = std::move(name);
		candidate.seconds = startSeconds;
		candidate.speed = playbackSpeed;
		candidate.loop = repeat;
		candidate.playing = true;
		candidate.nextClip.clear();
		candidate.nextSeconds = 0;
		candidate.blendDuration = 0;
		candidate.blendElapsed = 0;
		candidate.Validate();
		swap(*this, candidate);
	}

	void Animator::CrossFade(std::string name, double duration, bool repeat, double startSeconds)
	{
		if (name.empty() || !std::isfinite(duration) || duration < 0)
		{
			throw std::invalid_argument("AnimatorのCrossFadeにはClip名と有限の0以上の遷移秒数が必要です。");
		}
		if (duration == 0)
		{
			Play(std::move(name), repeat, speed, startSeconds);
			return;
		}
		Animator candidate = *this;
		candidate.nextClip = std::move(name);
		candidate.nextSeconds = startSeconds;
		candidate.nextLoop = repeat;
		candidate.blendDuration = duration;
		candidate.blendElapsed = 0;
		candidate.playing = true;
		candidate.Validate();
		swap(*this, candidate);
	}

	void Animator::Seek(double time)
	{
		if (!std::isfinite(time))
		{
			throw std::invalid_argument("AnimatorのSeekには有限の秒数が必要です。");
		}
		seconds = time;
	}

	void Animator::Advance(double deltaSeconds)
	{
		Validate();
		if (!std::isfinite(deltaSeconds) || deltaSeconds < 0)
		{
			throw std::invalid_argument("Animatorの経過時間には有限の0以上の秒数が必要です。");
		}
		if (!enabled || !playing)
		{
			return;
		}
		const double advanced = seconds + deltaSeconds * speed;
		const double nextAdvanced = nextSeconds + deltaSeconds * speed;
		if (!std::isfinite(advanced) || !std::isfinite(nextAdvanced))
		{
			throw std::overflow_error("Animatorの再生時刻がdoubleの有限範囲を超えています。");
		}
		seconds = advanced;
		if (blendDuration > 0)
		{
			nextSeconds = nextAdvanced;
			blendElapsed = deltaSeconds >= blendDuration - blendElapsed ? blendDuration : blendElapsed + deltaSeconds;
			if (blendElapsed == blendDuration)
			{
				clip.swap(nextClip);
				seconds = nextSeconds;
				loop = nextLoop;
				nextClip.clear();
				blendDuration = 0;
				blendElapsed = 0;
			}
		}
	}

	bool Animator::IsFinished(double duration) const
	{
		Validate();
		if (!std::isfinite(duration) || duration <= 0)
		{
			throw std::invalid_argument("Animatorの終了判定には有限の正のClip秒数が必要です。");
		}
		return !loop && blendDuration == 0 && (speed < 0 ? seconds <= 0 : seconds >= duration);
	}

	void swap(Animator& first, Animator& second) noexcept
	{
		first.clip.swap(second.clip);
		first.nextClip.swap(second.nextClip);
		std::swap(first.seconds, second.seconds);
		std::swap(first.speed, second.speed);
		std::swap(first.loop, second.loop);
		std::swap(first.playing, second.playing);
		std::swap(first.enabled, second.enabled);
		std::swap(first.nextSeconds, second.nextSeconds);
		std::swap(first.nextLoop, second.nextLoop);
		std::swap(first.blendDuration, second.blendDuration);
		std::swap(first.blendElapsed, second.blendElapsed);
	}
}
