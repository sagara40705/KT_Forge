#include <World/Systems/TransformSystem.h>
#include <Core/Math/Matrix4.h>
#include <utility>

namespace KT::World
{
	void TransformSystem::Update(SceneUpdateContext& context) const
	{
		context.RequireStage(SceneUpdateContext::Stage::Activation);
		try
		{
			std::vector<FinalizedEntity> result(context.inputs_.size());
			for (auto i : context.hierarchy_.parentFirst)
			{
				const auto& input = context.inputs_[i];
				const auto parent = context.hierarchy_.nodes[i].parent;
				const auto local = KT::Core::Math::LocalMatrix(input.local.position, input.local.rotation, input.local.scale);
				result[i] = {input.entity, {context.active_[i]},
					{parent == NoParent ? local : KT::Core::Math::Multiply(local, result[parent].transform.matrix)}};
			}
			context.frame_.entities = std::move(result);
			context.stage_ = SceneUpdateContext::Stage::Transform;
		}
		catch (...)
		{
			context.Fail();
			throw;
		}
	}
}
