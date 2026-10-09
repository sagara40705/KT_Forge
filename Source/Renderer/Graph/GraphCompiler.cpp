#include <Renderer/Graph/GraphCompiler.h>
#include <stdexcept>
#include <utility>

namespace KT::Renderer
{
	// Resolver/Validatorの検査済み入力から、登録順の遷移計画を作る。Storageは変更しない。
	GraphCompiledPlan GraphCompiler::Compile(const GraphStorage& storage, std::span<const GraphResolvedPass> passes) const
	{
		GraphCompiledPlan plan{};
		std::vector<D3D12_RESOURCE_STATES> currentStates{};

		// 前検査・初期状態：GPU Compileでは全登録画像のimportを要求する。
		for (const auto& resourceRecord : storage.resources)
		{
			if (!resourceRecord.importedTexture.has_value())
			{
				throw std::invalid_argument("リソースがインポートされていません");
			}

			currentStates.push_back(resourceRecord.importedTexture->initialState);
		}

		// パス計画：宣言順に必要状態を追跡する。
		for (const auto& resolvedPass : passes)
		{
			GraphPlannedPass plannedPass{};
			plannedPass.passIndex = resolvedPass.passIndex;

			for (const auto& use : resolvedPass.uses)
			{
				// 必要状態：用途からRTまたは書込可能depthの状態を選ぶ。
				D3D12_RESOURCE_STATES requiredState = D3D12_RESOURCE_STATE_COMMON;

				switch (use.usage)
				{
				case GraphResourceUsage::RenderTarget:
					requiredState = D3D12_RESOURCE_STATE_RENDER_TARGET;
					break;

				case GraphResourceUsage::DepthStencil:
					requiredState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
					break;

				// 初版GPUで状態を決められない用途は拒否する。
				case GraphResourceUsage::Unspecified:
				default:
					throw std::invalid_argument("未指定の使用用途は拒否されました");
				}

				// 遷移の登録：状態が変わる場合だけバリアを計画する。
				D3D12_RESOURCE_STATES currentState = currentStates[use.resource.index];
				if (currentState != requiredState)
				{
					GraphTransition transition{};
					transition.resource = use.resource;
					transition.range = use.range;
					transition.before = currentState;
					transition.after = requiredState;
					plannedPass.transitions.push_back(std::move(transition));
					// 状態更新：以降のパスは今回の遷移後の状態を基準にする。
					currentStates[use.resource.index] = requiredState;
				}
			}
			// パスの登録：遷移が0件でもcallbackの実行単位を残す。
			plan.passes.push_back(std::move(plannedPass));
		}

		// 終了遷移：現在状態と指定finalStateが違う画像だけ計画する。
		for (std::size_t index = 0; index < storage.resources.size(); ++index)
		{
			const auto& importedTexture = storage.resources[index].importedTexture;

			D3D12_RESOURCE_STATES before = currentStates[index];
			D3D12_RESOURCE_STATES after = importedTexture->finalState;
			if (before != after)
			{
				GraphTransition transition{};
				transition.resource = GraphResourceHandle{storage.graphid, static_cast<std::uint32_t>(index)};
				transition.range = GraphTextureRange{};
				transition.before = before;
				transition.after = after;
				plan.finalTransitions.push_back(std::move(transition));
			}
		}

		return plan;
	}
}
