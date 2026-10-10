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
			// 親のDirtyを子へ伝播し、必要な部分木だけLocal * Parentで再計算する。
			auto transforms = context.frame_.entities;
			auto dirty = context.transformDirty_;
			std::size_t calculated = 0;
			for (auto nodeIndex : context.hierarchy_->parentFirst)
			{
				const auto& input = context.inputs_[nodeIndex];
				const auto parentIndex = context.hierarchy_->nodes[nodeIndex].parent;

				dirty[nodeIndex] = dirty[nodeIndex] || (parentIndex != NoParent && dirty[parentIndex]);
				if (dirty[nodeIndex])
				{
					// 変更入力は従来と同じMath APIで検証し、無効なLocalや合成overflowを拒否する。
					const auto localMatrix = KT::Core::Math::LocalMatrix(input.local.position, input.local.rotation, input.local.scale);
					transforms[nodeIndex].transform.matrix = parentIndex == NoParent ? localMatrix :
						KT::Core::Math::Multiply(localMatrix, transforms[parentIndex].transform.matrix);
					++calculated;
				}

				// ActiveSelfだけの変更でも完成結果の有効状態を反映する。
				transforms[nodeIndex].entity = input.entity;
				transforms[nodeIndex].active.value = context.active_[nodeIndex];
			}

			// 全件の計算後に結果と更新段階を反映する。
			context.frame_.entities = std::move(transforms);
			context.transformDirty_ = std::move(dirty);
			context.statistics_.transformCalculated = calculated;
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
