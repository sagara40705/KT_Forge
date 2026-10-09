#include <Renderer/Graph/GraphCompiler.h>
#include <stdexcept>
#include <utility>

namespace KT::Renderer
{
    GraphCompiledPlan GraphCompiler::Compile(const GraphStorage& storage, std::span<const GraphResolvedPass> passes) const
    {
		GraphCompiledPlan plan{};
        std::vector<D3D12_RESOURCE_STATES> currentStates{};

		// 各リソースの初期状態をcurrentStatesに追加する
		for (const auto& resourceRecord : storage.resources)
		{
			if (!resourceRecord.importedTexture.has_value())
			{
				throw std::invalid_argument("リソースがインポートされていません");
			}

			// 初期状態をcurrentStatesに追加
			currentStates.push_back(resourceRecord.importedTexture->initialState);
		}

		//引数passesを登録順にループする
		for (const auto& resolvedPass : passes)
		{
			GraphPlannedPass plannedPass{};
			plannedPass.passIndex = resolvedPass.passIndex;

			// 各使用をループする
			for (const auto& use : resolvedPass.uses)
			{
				// 必要なD3D12_RESOURCE_STATES
				D3D12_RESOURCE_STATES requiredState = D3D12_RESOURCE_STATE_COMMON;

				switch (use.usage)
				{
				case GraphResourceUsage::RenderTarget:
					requiredState = D3D12_RESOURCE_STATE_RENDER_TARGET;
					break;

				case GraphResourceUsage::DepthStencil:
					requiredState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
					break;

				// エラー処理: 未指定の使用用途は拒否する
				case GraphResourceUsage::Unspecified:
				default:
					throw std::invalid_argument("未指定の使用用途は拒否されました");
				}

				// 現在状態と必要状態を比較する
				D3D12_RESOURCE_STATES currentState = currentStates[use.resource.index];
				if (currentState != requiredState)
				{
					// 遷移を追加する
					GraphTransition transition{};
					transition.resource = use.resource;
					transition.range = use.range;
					transition.before= currentState;
					transition.after = requiredState;
					plannedPass.transitions.push_back(std::move(transition));
					// 現在状態を更新する
					currentStates[use.resource.index] = requiredState;
				}
			}
			plan.passes.push_back(std::move(plannedPass));
		}

		// 画像を指定されたfinalStateへ戻す計画を追加する
		for (std::size_t index = 0; index < storage.resources.size(); ++index)
		{
			// 各リソースのimportedTextureを参照
			const auto& importedTexture = storage.resources[index].importedTexture;

			// 最終状態を比較する
			D3D12_RESOURCE_STATES before = currentStates[index];
			D3D12_RESOURCE_STATES after = importedTexture->finalState;
			if (before != after)
			{
				// 遷移を追加する
				GraphTransition transition{};
				transition.resource = GraphResourceHandle{ storage.graphid, static_cast<std::uint32_t>(index) };
				transition.range = GraphTextureRange{};
				transition.before = before;
				transition.after = after;
				plan.finalTransitions.push_back(std::move(transition));
			}
		}

		return plan;
	}
}
