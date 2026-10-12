#include <Animation/SkeletonAnimation.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/transform.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace KT::Animation
{
	struct SkeletonAnimation::Data
	{
		// 元定義とozzの並替え対応を、不変素材とともに保持する。
		std::shared_ptr<const KT::Content::SkinDefinition> definition;
		ozz::unique_ptr<ozz::animation::Skeleton> skeleton;
		std::vector<std::size_t> sourceByRuntime;
		std::vector<std::size_t> runtimeBySource;
		std::map<std::string, ozz::unique_ptr<ozz::animation::Animation>, std::less<>> clips;
	};

	SkeletonAnimation::SkeletonAnimation(std::shared_ptr<const KT::Content::SkinDefinition> definition)
		: data_(std::make_unique<Data>())
	{
		if (!definition)
		{
			throw std::invalid_argument("AnimationのSkin定義が空です。");
		}
		definition->Validate();
		data_->definition = std::move(definition);
		const auto& source = *data_->definition;

		// 検査済みの最大深さ128の階層をozzのRawSkeletonへコピーする。
		ozz::animation::offline::RawSkeleton rawSkeleton;
		const auto appendChildren = [&](auto&& self, std::int32_t parent, auto& destination) -> void
		{
			for (std::size_t index = 0; index < source.joints.size(); ++index)
			{
				const auto& joint = source.joints[index];
				if (joint.parent != parent)
				{
					continue;
				}
				destination.emplace_back();
				auto& output = destination.back();
				output.name = std::to_string(index).c_str();
				const auto rotation = KT::Core::Math::Normalize(joint.rotation);
				output.transform = {{joint.translation.x, joint.translation.y, joint.translation.z},
					{rotation.x, rotation.y, rotation.z, rotation.w}, {joint.scale.x, joint.scale.y, joint.scale.z}};
				self(self, static_cast<std::int32_t>(index), output.children);
			}
		};
		appendChildren(appendChildren, -1, rawSkeleton.roots);
		data_->skeleton = ozz::animation::offline::SkeletonBuilder{}(rawSkeleton);
		if (!data_->skeleton)
		{
			throw std::invalid_argument("ozz SkeletonBuilderで骨格を生成できません。");
		}
		data_->runtimeBySource.resize(source.joints.size());
		for (const auto name : data_->skeleton->joint_names())
		{
			const auto index = std::stoul(name);
			data_->runtimeBySource[index] = data_->sourceByRuntime.size();
			data_->sourceByRuntime.push_back(index);
		}

		// 空チャンネルへrest値を補い、Skeletonと同じ順で全Clipを圧縮する。
		for (const auto& clip : source.clips)
		{
			ozz::animation::offline::RawAnimation rawAnimation;
			rawAnimation.duration = clip.duration;
			rawAnimation.name = clip.name.c_str();
			rawAnimation.tracks.resize(source.joints.size());
			for (std::size_t index = 0; index < rawAnimation.tracks.size(); ++index)
			{
				const auto sourceIndex = data_->sourceByRuntime[index];
				const auto& track = clip.tracks[sourceIndex];
				const auto& rest = source.joints[sourceIndex];
				auto& output = rawAnimation.tracks[index];
				for (const auto& key : track.translations)
				{
					output.translations.push_back({key.time, {key.value.x, key.value.y, key.value.z}});
				}
				for (const auto& key : track.rotations)
				{
					const auto rotation = KT::Core::Math::Normalize(key.value);
					output.rotations.push_back({key.time, {rotation.x, rotation.y, rotation.z, rotation.w}});
				}
				for (const auto& key : track.scales)
				{
					output.scales.push_back({key.time, {key.value.x, key.value.y, key.value.z}});
				}
				if (output.translations.empty())
				{
					output.translations.push_back({0, {rest.translation.x, rest.translation.y, rest.translation.z}});
				}
				if (output.rotations.empty())
				{
					const auto rotation = KT::Core::Math::Normalize(rest.rotation);
					output.rotations.push_back({0, {rotation.x, rotation.y, rotation.z, rotation.w}});
				}
				if (output.scales.empty())
				{
					output.scales.push_back({0, {rest.scale.x, rest.scale.y, rest.scale.z}});
				}
			}
			auto animation = ozz::animation::offline::AnimationBuilder{}(rawAnimation);
			if (!animation)
			{
				throw std::invalid_argument("ozz AnimationBuilderでClipを生成できません: " + clip.name);
			}
			data_->clips.emplace(clip.name, std::move(animation));
		}
	}

	SkeletonAnimation::~SkeletonAnimation() = default;

	float SkeletonAnimation::Duration(std::string_view clip) const
	{
		const auto found = data_->clips.find(clip);
		if (found == data_->clips.end())
		{
			throw std::invalid_argument("Animation Clipがありません: " + std::string(clip));
		}
		return found->second->duration();
	}

	const KT::Content::SkinDefinition& SkeletonAnimation::Definition() const noexcept
	{
		return *data_->definition;
	}

	struct PoseSampler::Data
	{
		std::shared_ptr<const SkeletonAnimation> animation;
		// 二つのClipは別Contextで評価し、個体間にも可変領域を共有しない。
		ozz::animation::SamplingJob::Context firstContext, secondContext;
		ozz::vector<ozz::math::SoaTransform> firstPose, secondPose, blendedPose;
		ozz::vector<ozz::math::Float4x4> models;
	};

	PoseSampler::PoseSampler(std::shared_ptr<const SkeletonAnimation> animation)
		: data_(std::make_unique<Data>())
	{
		if (!animation)
		{
			throw std::invalid_argument("PoseSamplerの共有Animationが空です。");
		}
		data_->animation = std::move(animation);
		const auto& skeleton = *data_->animation->data_->skeleton;
		data_->firstContext.Resize(skeleton.num_joints());
		data_->secondContext.Resize(skeleton.num_joints());
		data_->firstPose.resize(skeleton.num_soa_joints());
		data_->secondPose.resize(skeleton.num_soa_joints());
		data_->blendedPose.resize(skeleton.num_soa_joints());
		data_->models.resize(skeleton.num_joints());
	}

	PoseSampler::~PoseSampler() = default;

	std::vector<KT::Core::Math::Matrix4> PoseSampler::Sample(
		std::string_view clip, double seconds, bool loop, std::string_view nextClip, double nextSeconds, bool nextLoop, float blend)
	{
		if (!std::isfinite(seconds) || !std::isfinite(nextSeconds) || !std::isfinite(blend) || blend < 0 || blend > 1)
		{
			throw std::invalid_argument("Animationの秒数またはblend係数が不正です。");
		}
		const auto& shared = *data_->animation->data_;
		const auto& skeleton = *shared.skeleton;
		const auto sample = [&](std::string_view name, double time, bool repeat, auto& context, auto& output)
		{
			if (name.empty())
			{
				std::copy(skeleton.joint_rest_poses().begin(), skeleton.joint_rest_poses().end(), output.begin());
				return;
			}
			const auto found = shared.clips.find(name);
			if (found == shared.clips.end())
			{
				throw std::invalid_argument("Animation Clipがありません: " + std::string(name));
			}
			const double duration = found->second->duration();
			double position = repeat ? std::fmod(time, duration) : std::clamp(time, 0.0, duration);
			if (position < 0)
			{
				position += duration;
			}
			ozz::animation::SamplingJob job;
			job.animation = found->second.get();
			job.context = &context;
			job.ratio = static_cast<float>(position / duration);
			job.output = ozz::make_span(output);
			if (!job.Run())
			{
				throw std::runtime_error("ozz SamplingJobの姿勢評価に失敗しました。");
			}
		};
		sample(clip, seconds, loop, data_->firstContext, data_->firstPose);
		auto localPose = ozz::make_span(data_->firstPose);
		if (blend > 0)
		{
			sample(nextClip, nextSeconds, nextLoop, data_->secondContext, data_->secondPose);
			std::array<ozz::animation::BlendingJob::Layer, 2> layers;
			layers[0].weight = 1 - blend;
			layers[0].transform = ozz::make_span(data_->firstPose);
			layers[1].weight = blend;
			layers[1].transform = ozz::make_span(data_->secondPose);
			ozz::animation::BlendingJob job;
			job.layers = ozz::make_span(layers);
			job.rest_pose = skeleton.joint_rest_poses();
			job.output = ozz::make_span(data_->blendedPose);
			if (!job.Run())
			{
				throw std::runtime_error("ozz BlendingJobの姿勢ブレンドに失敗しました。");
			}
			localPose = ozz::make_span(data_->blendedPose);
		}

		// ローカル姿勢からモデル行列を求め、column-vector表現をrow-vectorへ転置する。
		ozz::animation::LocalToModelJob modelJob;
		modelJob.skeleton = &skeleton;
		modelJob.input = localPose;
		modelJob.output = ozz::make_span(data_->models);
		if (!modelJob.Run())
		{
			throw std::runtime_error("ozz LocalToModelJobの骨行列生成に失敗しました。");
		}
		std::vector<KT::Core::Math::Matrix4> palette;
		palette.reserve(shared.definition->bindings.size());
		for (const auto& binding : shared.definition->bindings)
		{
			KT::Core::Math::Matrix4 model;
			const auto& source = data_->models[shared.runtimeBySource[binding.joint]];
			for (std::size_t column = 0; column < 4; ++column)
			{
				ozz::math::StorePtrU(source.cols[column], &model.elements[column * 4]);
			}
			const auto matrix = binding.inverseBind * model * shared.definition->modelToMesh;
			if (!KT::Core::Math::IsAffine(matrix))
			{
				throw std::runtime_error("Animation評価後のSkin行列を有限Affineで保持できません。");
			}
			palette.push_back(matrix);
		}
		return palette;
	}
}
