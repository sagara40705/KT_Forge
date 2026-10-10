#include <World/Scene/SceneUpdateContext.h>
#include <bit>
#include <stdexcept>

namespace KT::World
{
	SceneUpdateContext::SceneUpdateContext(const World& world, std::optional<Entity> camera, Viewport viewport,
		std::uint64_t updateNumber, const SceneUpdateContext* previous)
		: worldId_(world.Identity()),
		  updateNumber_(updateNumber),
		  camera_(camera),
		  viewport_(viewport)
	{
		// 完成した同じWorldの結果だけを比較元として受け付ける。
		if (previous)
		{
			previous->RequireAtLeast(Stage::Transform);
			if (previous->worldId_ != worldId_)
			{
				throw std::invalid_argument("差分更新の比較元が別のWorldのCPU結果です。");
			}
		}

		// 階層と同じslot順で値を捕捉し、以降のSystemでWorldを再読取しない。
		hierarchy_ = world.GetHierarchy();
		inputs_.reserve(hierarchy_->nodes.size());

		for (const auto& node : hierarchy_->nodes)
		{
			const auto entity = node.entity;
			SceneEntityInput input;
			input.entity = entity;
			input.changes = world.GetCpuChangeState(entity);

			// 任意componentがある場合だけ、既定値を入力で置き換える。
			input.hierarchy.parent = node.parent == NoParent ? Entity{} : hierarchy_->nodes[node.parent].entity;
			if (const auto* activeSelf = world.FindComponent<ActiveSelf>(entity))
			{
				input.activeSelf = *activeSelf;
			}
			if (const auto* localTransform = world.FindComponent<LocalTransform>(entity))
			{
				input.local = *localTransform;
			}
			if (const auto* cameraComponent = world.FindComponent<Camera>(entity))
			{
				input.camera = *cameraComponent;
			}
			if (const auto* meshRenderer = world.FindComponent<MeshRenderer>(entity))
			{
				input.mesh = *meshRenderer;
			}

			inputs_.push_back(input);
		}

		// 新規Entityは全計算し、生存世代が一致した入力だけ前回値を再利用する。
		activationDirty_.assign(inputs_.size(), true);
		transformDirty_.assign(inputs_.size(), true);
		active_.resize(inputs_.size());
		frame_.entities.resize(inputs_.size());
		if (previous)
		{
			for (std::size_t nodeIndex = 0; nodeIndex < inputs_.size(); ++nodeIndex)
			{
				const auto& input = inputs_[nodeIndex];
				if (input.entity.index >= previous->hierarchy_->nodeByEntityIndex.size())
				{
					continue;
				}

				const auto previousIndex = previous->hierarchy_->nodeByEntityIndex[input.entity.index];
				if (previousIndex == NoParent || previous->inputs_[previousIndex].entity != input.entity)
				{
					continue;
				}

				const auto& oldInput = previous->inputs_[previousIndex];
				activationDirty_[nodeIndex] = false;
				transformDirty_[nodeIndex] = false;

				// 通知済み入力は世代一致で値比較を省く。公開した可変参照は毎回照合する。
				if (input.changes.activationBorrowed || oldInput.changes.activationBorrowed ||
					input.changes.versions.activation != oldInput.changes.versions.activation)
				{
					++statistics_.activationCompared;
					activationDirty_[nodeIndex] = input.hierarchy.parent != oldInput.hierarchy.parent ||
						input.activeSelf.value != oldInput.activeSelf.value;
				}

				// floatの各値を照合し、負の0も検出する。structのpaddingは比較しない。
				const auto sameFloat = [](float first, float second)
				{
					return std::bit_cast<std::uint32_t>(first) == std::bit_cast<std::uint32_t>(second);
				};
				if (input.changes.transformBorrowed || oldInput.changes.transformBorrowed ||
					input.changes.versions.transform != oldInput.changes.versions.transform)
				{
					++statistics_.transformCompared;
					const auto& local = input.local;
					const auto& oldLocal = oldInput.local;
					transformDirty_[nodeIndex] = input.hierarchy.parent != oldInput.hierarchy.parent ||
						!sameFloat(local.position.x, oldLocal.position.x) || !sameFloat(local.position.y, oldLocal.position.y) ||
						!sameFloat(local.position.z, oldLocal.position.z) || !sameFloat(local.rotation.x, oldLocal.rotation.x) ||
						!sameFloat(local.rotation.y, oldLocal.rotation.y) || !sameFloat(local.rotation.z, oldLocal.rotation.z) ||
						!sameFloat(local.rotation.w, oldLocal.rotation.w) || !sameFloat(local.scale.x, oldLocal.scale.x) ||
						!sameFloat(local.scale.y, oldLocal.scale.y) || !sameFloat(local.scale.z, oldLocal.scale.z);
				}

				active_[nodeIndex] = previous->active_[previousIndex];
				frame_.entities[nodeIndex] = previous->frame_.entities[previousIndex];
			}
		}
	}

	void SceneUpdateContext::RequireStage(Stage expected) const
	{
		if (stage_ != expected)
		{
			throw std::logic_error("Sceneの更新処理順が不正、またはこのCPU更新が失敗済みです。");
		}
	}

	void SceneUpdateContext::RequireAtLeast(Stage minimum) const
	{
		if (stage_ == Stage::Failed || stage_ < minimum)
		{
			throw std::logic_error("要求したSceneのCPU結果が未完成、またはこのCPU更新が失敗済みです。");
		}
	}

	const std::vector<SceneEntityInput>& SceneUpdateContext::Inputs() const
	{
		RequireAtLeast(Stage::Captured);
		return inputs_;
	}

	const HierarchySnapshot& SceneUpdateContext::GetHierarchy() const
	{
		RequireAtLeast(Stage::Hierarchy);
		return *hierarchy_;
	}

	const std::vector<bool>& SceneUpdateContext::GetActivation() const
	{
		RequireAtLeast(Stage::Activation);
		return active_;
	}

	const std::vector<FinalizedEntity>& SceneUpdateContext::GetTransforms() const
	{
		RequireAtLeast(Stage::Transform);
		return frame_.entities;
	}

	const WorldFrame& SceneUpdateContext::GetFrame() const
	{
		RequireStage(Stage::Camera);
		return frame_;
	}

	const WorldFrame& SceneUpdateContext::GetCpuFrame() const
	{
		RequireAtLeast(Stage::Transform);
		return frame_;
	}

	const CpuUpdateStatistics& SceneUpdateContext::GetCpuUpdateStatistics() const
	{
		RequireAtLeast(Stage::Transform);
		return statistics_;
	}
}
