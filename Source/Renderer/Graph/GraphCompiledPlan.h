#pragma once
#include <Renderer/Graph/GraphResourceUse.h>
#include <d3d12.h>
#include <cstddef>
#include <vector>

namespace KT::Renderer
{
	// Viewから親画像/rangeへ正規化した使用。元passのview宣言もStorageに残す。
	struct GraphNormalizedUse
	{
		GraphResourceHandle resource{};
		GraphTextureRange range{};
		GraphResourceAccess access = GraphResourceAccess::Read;
		GraphResourceUsage usage = GraphResourceUsage::Unspecified;
	};
	struct GraphResolvedPass
	{
		std::size_t passIndex = 0;
		std::vector<GraphNormalizedUse> uses;
	};
	struct GraphTransition
	{
		GraphResourceHandle resource{};
		GraphTextureRange range{};
		D3D12_RESOURCE_STATES before = D3D12_RESOURCE_STATE_COMMON;
		D3D12_RESOURCE_STATES after = D3D12_RESOURCE_STATE_COMMON;
	};
	struct GraphPlannedPass
	{
		std::size_t passIndex = 0;
		std::vector<GraphTransition> transitions;
	};
	// 単一Direct Queue/登録順。計画の完成とGPU実行/完了は別。
	// 部分range遷移、UAV/aliasing barrier、自動並替え/cullingは初版で扱わない。
	struct GraphCompiledPlan
	{
		std::vector<GraphPlannedPass> passes;
		std::vector<GraphTransition> finalTransitions;
	};
}
