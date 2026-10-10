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
			// 親のDirtyを子へ伝播し、影響のないEntityは捕捉時にコピーした結果を使う。
			auto activeStates = context.active_;
			auto dirty = context.activationDirty_;
			std::size_t calculated = 0;
			for (auto nodeIndex : context.hierarchy_->parentFirst)
			{
				const auto parentIndex = context.hierarchy_->nodes[nodeIndex].parent;
				dirty[nodeIndex] = dirty[nodeIndex] || (parentIndex != NoParent && dirty[parentIndex]);
				if (dirty[nodeIndex])
				{
					activeStates[nodeIndex] =
						context.inputs_[nodeIndex].activeSelf.value && (parentIndex == NoParent || activeStates[parentIndex]);
					++calculated;
				}
			}

			// 全件の計算後に結果と更新段階を反映する。
			context.active_ = std::move(activeStates);
			context.activationDirty_ = std::move(dirty);
			context.statistics_.activationCalculated = calculated;
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
