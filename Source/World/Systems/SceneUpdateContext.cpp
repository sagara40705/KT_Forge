#include <World/Systems/SceneUpdateContext.h>
#include <stdexcept>

namespace KT::World
{
	SceneUpdateContext::SceneUpdateContext(const World& world, std::optional<Entity> camera, Viewport viewport)
		: worldId_(world.Identity()),
		  camera_(camera),
		  viewport_(viewport)
	{
		const auto entities = world.Entities();
		inputs_.reserve(entities.size());
		for (auto entity : entities)
		{
			SceneEntityInput input;
			input.entity = entity;
			if (const auto* value = world.FindComponent<Hierarchy>(entity))
			{
				input.hierarchy = *value;
			}
			if (const auto* value = world.FindComponent<ActiveSelf>(entity))
			{
				input.activeSelf = *value;
			}
			if (const auto* value = world.FindComponent<LocalTransform>(entity))
			{
				input.local = *value;
			}
			if (const auto* value = world.FindComponent<Camera>(entity))
			{
				input.camera = *value;
			}
			if (const auto* value = world.FindComponent<MeshRenderer>(entity))
			{
				input.mesh = *value;
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
