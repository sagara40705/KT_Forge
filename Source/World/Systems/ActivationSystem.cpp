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
			// 親から順に有効状態を計算し、無効な親の子も無効にする。
			std::vector<bool> activeStates(context.inputs_.size());
			for (auto nodeIndex : context.hierarchy_->parentFirst)
			{
				const auto parentIndex = context.hierarchy_->nodes[nodeIndex].parent;
				activeStates[nodeIndex] =
					context.inputs_[nodeIndex].activeSelf.value && (parentIndex == NoParent || activeStates[parentIndex]);
			}

			// 全件の計算後に結果と更新段階を反映する。
			context.active_ = std::move(activeStates);
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
