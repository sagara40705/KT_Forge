#include <World/Scene/SceneUpdateContext.h>
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace KT::World
{
	SceneUpdateContext::SceneUpdateContext(const World& world, std::optional<Entity> camera, Viewport viewport,
		std::uint64_t updateNumber, const SceneUpdateContext* previous)
	{
		Capture(world, camera, viewport, updateNumber, previous);
	}

	void SceneUpdateContext::Capture(const World& world, std::optional<Entity> camera, Viewport viewport,
		std::uint64_t updateNumber, const SceneUpdateContext* previous)
	{
		// 完成した同じWorldの結果だけを比較元として受け付ける。
		if (previous)
		{
			previous->RequireAtLeast(Stage::Transform);
			if (previous == this || previous->worldId_ != world.Identity())
			{
				throw std::invalid_argument("差分更新の比較元が同じcontext、または別のWorldのCPU結果です。");
			}
		}

		// 公開・比較中でないcontextの配列容量を再利用する。捕捉失敗時は参照を禁止する。
		stage_ = Stage::Failed;
		worldId_ = world.Identity();
		updateNumber_ = updateNumber;
		camera_ = camera;
		viewport_ = viewport;
		statistics_ = {};
		frame_.camera.reset();
		hierarchy_ = world.GetHierarchy();
		inputs_.resize(hierarchy_->nodes.size());
		active_.resize(inputs_.size());
		frame_.entities.resize(inputs_.size());
		activationSeeds_.clear();
		transformSeeds_.clear();

		for (std::size_t nodeIndex = 0; nodeIndex < inputs_.size(); ++nodeIndex)
		{
			const auto& node = hierarchy_->nodes[nodeIndex];
			const auto entity = node.entity;
			std::size_t oldIndex = NoParent;
			if (previous && entity.index < previous->hierarchy_->nodeByEntityIndex.size())
			{
				oldIndex = previous->hierarchy_->nodeByEntityIndex[entity.index];
				if (oldIndex != NoParent && previous->inputs_[oldIndex].entity != entity)
				{
					oldIndex = NoParent;
				}
			}
			const auto* oldInput = oldIndex == NoParent ? nullptr : &previous->inputs_[oldIndex];
			SceneEntityInput input;
			input.entity = entity;
			input.changes = world.GetCpuChangeState(entity);
			input.hierarchy.parent = node.parent == NoParent ? Entity{} : hierarchy_->nodes[node.parent].entity;
			const bool readActivation = !oldInput || input.changes.activationBorrowed || oldInput->changes.activationBorrowed ||
				input.changes.versions.activation != oldInput->changes.versions.activation;
			const bool readTransform = !oldInput || input.changes.transformBorrowed || oldInput->changes.transformBorrowed ||
				input.changes.versions.transform != oldInput->changes.versions.transform;

			// 世代一致かつ可変借用なしの場合は、Component検索を前回入力のコピーで置き換える。
			if (readActivation)
			{
				++statistics_.activationRead;
				if (const auto* active = world.FindComponent<ActiveSelf>(entity))
				{
					input.activeSelf = *active;
				}
			}
			else
			{
				input.activeSelf = oldInput->activeSelf;
			}
			if (readTransform)
			{
				++statistics_.transformRead;
				if (const auto* local = world.FindComponent<LocalTransform>(entity))
				{
					input.local = *local;
				}
			}
			else
			{
				input.local = oldInput->local;
			}
			// 変更世代の対象外は毎回捕捉する。
			if (const auto* component = world.FindComponent<Camera>(entity))
			{
				input.camera = *component;
			}
			if (const auto* component = world.FindComponent<MeshRenderer>(entity))
			{
				input.mesh = *component;
			}

			bool activationDirty = !oldInput;
			bool transformDirty = !oldInput;
			if (oldInput)
			{
				activationDirty = input.hierarchy.parent != oldInput->hierarchy.parent;
				transformDirty = activationDirty;
				if (readActivation)
				{
					++statistics_.activationCompared;
					activationDirty = activationDirty || input.activeSelf.value != oldInput->activeSelf.value;
				}
				if (readTransform)
				{
					++statistics_.transformCompared;
					// paddingを除き、負の0を含むfloatの各値を比較する。
					const auto sameFloat = [](float first, float second)
					{
						return std::bit_cast<std::uint32_t>(first) == std::bit_cast<std::uint32_t>(second);
					};
					const auto& local = input.local;
					const auto& oldLocal = oldInput->local;
					transformDirty = transformDirty ||
						!sameFloat(local.position.x, oldLocal.position.x) || !sameFloat(local.position.y, oldLocal.position.y) ||
						!sameFloat(local.position.z, oldLocal.position.z) || !sameFloat(local.rotation.x, oldLocal.rotation.x) ||
						!sameFloat(local.rotation.y, oldLocal.rotation.y) || !sameFloat(local.rotation.z, oldLocal.rotation.z) ||
						!sameFloat(local.rotation.w, oldLocal.rotation.w) || !sameFloat(local.scale.x, oldLocal.scale.x) ||
						!sameFloat(local.scale.y, oldLocal.scale.y) || !sameFloat(local.scale.z, oldLocal.scale.z);
				}
				active_[nodeIndex] = previous->active_[oldIndex];
				frame_.entities[nodeIndex] = previous->frame_.entities[oldIndex];
			}
			else
			{
				active_[nodeIndex] = false;
				frame_.entities[nodeIndex] = {};
			}
			frame_.entities[nodeIndex].entity = entity;
			inputs_[nodeIndex] = input;
			if (activationDirty)
			{
				activationSeeds_.push_back(nodeIndex);
			}
			if (transformDirty)
			{
				transformSeeds_.push_back(nodeIndex);
			}
		}
		CollectDirty(activationSeeds_, activationWork_);
		CollectDirty(transformSeeds_, transformWork_);
		stage_ = Stage::Captured;
	}

	void SceneUpdateContext::CollectDirty(std::vector<std::size_t>& seeds, std::vector<std::size_t>& work)
	{
		work.clear();
		std::sort(seeds.begin(), seeds.end(), [this](auto first, auto second)
		{
			return hierarchy_->subtreeBegin[first] < hierarchy_->subtreeBegin[second];
		});
		std::size_t coveredEnd = 0;
		for (auto node : seeds)
		{
			const auto begin = hierarchy_->subtreeBegin[node];
			const auto end = hierarchy_->subtreeEnd[node];
			if (begin < coveredEnd)
			{
				continue;
			}
			work.insert(work.end(), hierarchy_->subtreeOrder.begin() + begin, hierarchy_->subtreeOrder.begin() + end);
			coveredEnd = end;
		}
		// 全件更新はソートせず、部分更新も従来の幅優先順で数値検証する。
		if (work.size() == inputs_.size())
		{
			work.assign(hierarchy_->parentFirst.begin(), hierarchy_->parentFirst.end());
		}
		else
		{
			std::sort(work.begin(), work.end(), [this](auto first, auto second)
			{
				return hierarchy_->parentFirstPosition[first] < hierarchy_->parentFirstPosition[second];
			});
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
