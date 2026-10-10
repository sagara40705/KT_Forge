#include <World/Scene/SceneUpdateContext.h>
#include <stdexcept>

namespace KT::World
{
	SceneUpdateContext::SceneUpdateContext(const World& world, std::optional<Entity> camera, Viewport viewport)
		: worldId_(world.Identity()),
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
			throw std::logic_error("Scene System order is invalid or this update has failed.");
		}
	}

	void SceneUpdateContext::RequireAtLeast(Stage minimum) const
	{
		if (stage_ == Stage::Failed || stage_ < minimum)
		{
			throw std::logic_error("Requested Scene result is not complete.");
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
}
