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
			// 親を先に計算する部分木集合だけを、従来のMath APIで検証・計算する。
			for (auto nodeIndex : context.transformWork_)
			{
				const auto& input = context.inputs_[nodeIndex];
				const auto parentIndex = context.hierarchy_->nodes[nodeIndex].parent;
				const auto localMatrix = KT::Core::Math::LocalMatrix(input.local.position, input.local.rotation, input.local.scale);
				context.frame_.entities[nodeIndex].transform.matrix = parentIndex == NoParent ? localMatrix :
					KT::Core::Math::Multiply(localMatrix, context.frame_.entities[parentIndex].transform.matrix);
			}
			// ActiveSelfだけの変更も完成結果へ反映する。
			for (auto nodeIndex : context.activationWork_)
			{
				context.frame_.entities[nodeIndex].active.value = context.active_[nodeIndex];
			}
			context.statistics_.transformCalculated = context.transformWork_.size();
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
