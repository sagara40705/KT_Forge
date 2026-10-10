#include <World/Scene/SceneUpdateContext.h>
#include <stdexcept>

namespace KT::World
{
	SceneUpdateContext::SceneUpdateContext(const World& world, std::optional<Entity> camera, Viewport viewport, std::uint64_t updateNumber)
		: worldId_(world.Identity()),
		  updateNumber_(updateNumber),
		  camera_(camera),
		  viewport_(viewport)
	{
		// 生存Entityの入力をコピーし、以降のSystemでWorldを再読取しない。
		const auto entities = world.Entities();
		inputs_.reserve(entities.size());

		for (auto entity : entities)
		{
			SceneEntityInput input;
			input.entity = entity;

			// 任意componentがある場合だけ、既定値を入力で置き換える。
			if (const auto* hierarchy = world.FindComponent<Hierarchy>(entity))
			{
				input.hierarchy = *hierarchy;
			}
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
		return hierarchy_;
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
}
