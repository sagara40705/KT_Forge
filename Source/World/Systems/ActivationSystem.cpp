#include <World/Systems/ActivationSystem.h>
#include <utility>

namespace KT::World
{
	void ActivationSystem::Update(SceneUpdateContext& context) const
	{
		context.RequireStage(SceneUpdateContext::Stage::Hierarchy);
		try
		{
			std::vector<bool> result(context.inputs_.size());
			for (auto i : context.hierarchy_.parentFirst)
			{
				const auto parent = context.hierarchy_.nodes[i].parent;
				result[i] = context.inputs_[i].activeSelf.value && (parent == NoParent || result[parent]);
			}
			context.active_ = std::move(result);
			context.stage_ = SceneUpdateContext::Stage::Activation;
		}
		catch (...)
		{
			context.Fail();
			throw;
		}
	}
}
