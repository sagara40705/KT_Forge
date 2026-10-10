#include <World/Systems/ActivationSystem.h>
#include <utility>

namespace KT::World
{
	void ActivationSystem::Update(SceneUpdateContext& context) const
	{
		// 前段階の完了を確認し、同じ更新の結果だけを使う。
		context.RequireStage(SceneUpdateContext::Stage::Hierarchy);

		try
		{
			// 統合した部分木だけを処理する。失敗contextは外へ公開しない。
			for (auto nodeIndex : context.activationWork_)
			{
				const auto parentIndex = context.hierarchy_->nodes[nodeIndex].parent;
				context.active_[nodeIndex] = context.inputs_[nodeIndex].activeSelf.value &&
					(parentIndex == NoParent || context.active_[parentIndex]);
			}
			context.statistics_.activationCalculated = context.activationWork_.size();
			context.stage_ = SceneUpdateContext::Stage::Activation;
		}
		catch (...)
		{
			// 失敗した更新の続行を禁止し、元の例外を返す。
			context.Fail();
			throw;
		}
	}
}
