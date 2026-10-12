#include <RuntimeIntegration/Animation/AnimationExtraction.h>
#include <Core/Math/AABB.h>
#include <set>
#include <stdexcept>

namespace KT::RuntimeIntegration
{
	void AnimationExtraction::Apply(const KT::World::SceneUpdateContext& cpu, const KT::Content::AssetCatalog& assets, RenderFrame& frame)
	{
		const auto& inputs = cpu.Inputs();
		const auto& hierarchy = cpu.GetHierarchy();
		std::set<SamplerKey> used;
		std::map<SamplerKey, std::shared_ptr<const std::vector<KT::Core::Math::Matrix4>>> poses;
		for (std::size_t index = 0; index < frame.candidates.objects.size(); ++index)
		{
			auto& candidate = frame.candidates.objects[index];
			const auto mesh = assets.Resolve(candidate.mesh);
			if (!mesh.asset || !mesh.asset->Skin())
			{
				continue;
			}
			const auto source = frame.sources.at(index);
			const SamplerKey key{cpu.SourceWorldId(), source.index, source.generation, candidate.mesh.ToString()};
			used.insert(key);
			if (const auto existing = poses.find(key); existing != poses.end())
			{
				candidate.skinPalette = existing->second;
			}
			else
			{
				// Clip/Skeletonは素材ごとに共有し、二つのSampling Contextは個体ごとに所有する。
				auto animation = animations_.find(candidate.mesh.Value());
				if (animation == animations_.end())
				{
					animation =
						animations_.emplace(candidate.mesh.Value(), std::make_shared<KT::Animation::SkeletonAnimation>(mesh.asset->Skin()))
							.first;
				}
				auto sampler = samplers_.find(key);
				if (sampler == samplers_.end())
				{
					sampler = samplers_.emplace(key, std::make_unique<KT::Animation::PoseSampler>(animation->second)).first;
				}
				const KT::World::Animator* animator = nullptr;
				for (auto node = hierarchy.FindNode(source); node != KT::World::NoParent; node = hierarchy.nodes[node].parent)
				{
					if (inputs[node].animator)
					{
						animator = &*inputs[node].animator;
						break;
					}
				}
				std::vector<KT::Core::Math::Matrix4> palette;
				if (!animator || !animator->enabled)
				{
					palette = sampler->second->Sample({}, 0, false);
				}
				else
				{
					animator->Validate();
					const auto blend =
						animator->blendDuration > 0 ? static_cast<float>(animator->blendElapsed / animator->blendDuration) : 0;
					palette = sampler->second->Sample(animator->clip, animator->seconds, animator->loop, animator->nextClip,
						animator->nextSeconds, animator->nextLoop, blend);
				}
				candidate.skinPalette = std::make_shared<const std::vector<KT::Core::Math::Matrix4>>(std::move(palette));
				poses.emplace(key, candidate.skinPalette);
			}

			// bind poseのAABBで動いた頂点を隠さない。描画と同じWeightで完成姿勢の境界を求める。
			KT::Core::Math::AABB bounds;
			const auto& submesh = mesh.asset->Submeshes()[candidate.submesh];
			for (std::size_t offset = submesh.firstIndex; offset < std::size_t(submesh.firstIndex) + submesh.indexCount; ++offset)
			{
				const auto vertex = mesh.asset->Indices()[offset];
				const auto position = mesh.asset->Positions()[vertex];
				const auto& influence = mesh.asset->Influences()[vertex];
				KT::Core::Math::Vector3 posed{};
				for (std::size_t component = 0; component < 4; ++component)
				{
					const auto transformed =
						KT::Core::Math::TransformPoint(position, candidate.skinPalette->at(influence.joints[component]));
					posed.x += transformed.x * influence.weights[component];
					posed.y += transformed.y * influence.weights[component];
					posed.z += transformed.z * influence.weights[component];
				}
				const auto world = KT::Core::Math::TransformPoint(posed, candidate.world);
				if (!KT::Core::Math::IsFinite(world))
				{
					throw std::invalid_argument("Skin描画の完成頂点を有限World座標で保持できません。");
				}
				bounds = KT::Core::Math::Expanded(bounds, world);
			}
			candidate.worldBounds = bounds;
		}

		// 今回使わなかった個体の可変領域を捨てる。不変素材は固定Catalogの寿命で共有する。
		std::erase_if(samplers_, [&](const auto& entry) { return !used.contains(entry.first); });
	}
}
