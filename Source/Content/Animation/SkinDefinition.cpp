#include <Content/Animation/SkinDefinition.h>
#include <cmath>
#include <set>
#include <stdexcept>

namespace KT::Content
{
	void SkinDefinition::Validate() const
	{
		if (joints.empty() || joints.size() > MaxJoints || bindings.empty() || bindings.size() > MaxJoints)
		{
			throw std::invalid_argument("SkinのSkeletonとbindingには1..256要素が必要です。");
		}
		if (!KT::Core::Math::IsAffine(modelToMesh))
		{
			throw std::invalid_argument("SkinのmodelToMeshには有限のAffine行列が必要です。");
		}
		(void)KT::Core::Math::Inverse(modelToMesh);

		// 名前と親を検査し、循環・曖昧なchannel対応・深い再帰を拒否する。
		std::set<std::string> names;
		std::vector<unsigned> depths(joints.size());
		for (std::size_t index = 0; index < joints.size(); ++index)
		{
			const auto& joint = joints[index];
			if (joint.name.empty() || joint.name.size() > 256 || joint.name.find('\0') != std::string::npos ||
				!names.insert(joint.name).second)
			{
				throw std::invalid_argument("Skeletonのjoint名には重複しない1..256byteの名前が必要です。");
			}
			if (joint.parent < -1 || joint.parent >= static_cast<std::int32_t>(index))
			{
				throw std::invalid_argument("Skeletonのparentは-1または先行jointを指定してください。");
			}
			depths[index] = joint.parent < 0 ? 0 : depths[joint.parent] + 1;
			if (depths[index] > 128)
			{
				throw std::invalid_argument("Skeletonの階層深さが128を超えています。");
			}
			const auto local = KT::Core::Math::LocalMatrix(joint.translation, joint.rotation, joint.scale);
			(void)KT::Core::Math::Inverse(local);
		}
		for (const auto& binding : bindings)
		{
			if (binding.joint >= joints.size() || !KT::Core::Math::IsAffine(binding.inverseBind))
			{
				throw std::invalid_argument("Skin bindingのjoint範囲またはinverseBind行列が不正です。");
			}
			(void)KT::Core::Math::Inverse(binding.inverseBind);
		}

		// 全Clipの容量・時間順・値を、ozz builderへ渡す前に検査する。
		names.clear();
		std::size_t keyCount = 0;
		if (clips.size() > 256)
		{
			throw std::invalid_argument("SkinのClip数が256を超えています。");
		}
		for (const auto& clip : clips)
		{
			if (clip.name.empty() || clip.name.size() > 256 || clip.name.find('\0') != std::string::npos ||
				!names.insert(clip.name).second || !std::isfinite(clip.duration) || clip.duration <= 0 ||
				clip.tracks.size() != joints.size())
			{
				throw std::invalid_argument("Animation Clipの名前・秒数・joint track数が不正です。");
			}
			for (const auto& track : clip.tracks)
			{
				const auto checkKeys = [&](const auto& keys)
				{
					if (keys.size() > MaxKeys - keyCount)
					{
						throw std::invalid_argument("Animationのkey総数が100万を超えています。");
					}
					keyCount += keys.size();
					float previousTime = -1;
					for (const auto& key : keys)
					{
						if (!std::isfinite(key.time) || key.time < 0 || key.time > clip.duration || key.time <= previousTime ||
							!KT::Core::Math::IsFinite(key.value))
						{
							throw std::invalid_argument("Animation keyの時刻・昇順・有限値が不正です。");
						}
						previousTime = key.time;
					}
				};
				checkKeys(track.translations);
				checkKeys(track.rotations);
				checkKeys(track.scales);
				for (const auto& key : track.rotations)
				{
					(void)KT::Core::Math::Normalize(key.value);
				}
			}
		}
	}

	void SkinDefinition::ValidateInfluences(const std::vector<SkinInfluence>& influences, std::size_t vertexCount) const
	{
		if (influences.size() != vertexCount)
		{
			throw std::invalid_argument("Skinの影響配列はMeshの全頂点との一致が必要です。");
		}
		for (const auto& influence : influences)
		{
			double sum = 0;
			for (std::size_t index = 0; index < 4; ++index)
			{
				if (influence.joints[index] >= bindings.size() || !std::isfinite(influence.weights[index]) ||
					influence.weights[index] < 0 || influence.weights[index] > 1)
				{
					throw std::invalid_argument("Skinのjoint番号またはWeightの範囲が不正です。");
				}
				sum += influence.weights[index];
			}
			if (std::abs(sum - 1) > 0.0001)
			{
				throw std::invalid_argument("Skinの頂点Weightの合計は1である必要があります。");
			}
		}
	}
}
