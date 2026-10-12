#pragma once
#include <Content/Animation/SkinDefinition.h>
#include <Core/Utility/NonCopyable.h>
#include <memory>
#include <string_view>

namespace KT::Animation
{
	class PoseSampler;

	// 同じ素材のozz Skeleton/圧縮Clipを一度だけ生成し、複数個体で読取共有する。
	class SkeletonAnimation : private KT::Core::NonCopyable
	{
	public:
		explicit SkeletonAnimation(std::shared_ptr<const KT::Content::SkinDefinition> definition);
		~SkeletonAnimation();
		[[nodiscard]] float Duration(std::string_view clip) const;
		[[nodiscard]] const KT::Content::SkinDefinition& Definition() const noexcept;

	private:
		friend class PoseSampler;
		struct Data;
		std::unique_ptr<Data> data_;
	};

	// Sampling Contextと姿勢計算領域を個体ごとに所有する。再生時刻は呼出側の値を使う。
	class PoseSampler : private KT::Core::NonCopyable
	{
	public:
		explicit PoseSampler(std::shared_ptr<const SkeletonAnimation> animation);
		~PoseSampler();
		// 空clipはrest pose。blendは0..1で、第二Clipとのローカル姿勢ブレンドを行う。
		[[nodiscard]] std::vector<KT::Core::Math::Matrix4> Sample(std::string_view clip, double seconds, bool loop,
			std::string_view nextClip = {}, double nextSeconds = 0, bool nextLoop = true, float blend = 0);

	private:
		struct Data;
		std::unique_ptr<Data> data_;
	};
}
