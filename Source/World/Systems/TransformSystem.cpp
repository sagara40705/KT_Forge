#include <World/Systems/TransformSystem.h>
#include <Core/Math/Matrix4.h>
#include <utility>

namespace KT::World
{
	void TransformSystem::Update(SceneUpdateContext& context) const
	{
		// 前段階の完了を確認し、同じ更新の結果だけを使う。
		context.RequireStage(SceneUpdateContext::Stage::Activation);

		try
		{
			// 親のWorld行列を先に求め、Local * Parentの順で合成する。
			std::vector<FinalizedEntity> transforms(context.inputs_.size());
			for (auto nodeIndex : context.hierarchy_.parentFirst)
			{
				const auto& input = context.inputs_[nodeIndex];
				const auto parentIndex = context.hierarchy_.nodes[nodeIndex].parent;

				const auto localMatrix = KT::Core::Math::LocalMatrix(input.local.position, input.local.rotation, input.local.scale);
				transforms[nodeIndex] = {input.entity, {context.active_[nodeIndex]},
					{parentIndex == NoParent ? localMatrix :
											   KT::Core::Math::Multiply(localMatrix, transforms[parentIndex].transform.matrix)}};
			}

			// 全件の計算後に結果と更新段階を反映する。
			context.frame_.entities = std::move(transforms);
			context.stage_ = SceneUpdateContext::Stage::Transform;
		}
		catch (...)
		{
			// 失敗した更新の続行を禁止し、元の例外を返す。
			context.Fail();
			throw;
		}
	}
}
