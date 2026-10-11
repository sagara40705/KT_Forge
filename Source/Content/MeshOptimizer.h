#pragma once
#include <Content/MeshAsset.h>
#include <Content/MeshOptimization.h>

namespace KT::Content
{
	// 元Meshから独立した最適化結果と要素数を所有する。
	struct MeshOptimizationResult
	{
		MeshAsset mesh;
		MeshOptimizationStatistics statistics;
	};

	// 完成済み属性の配置だけを変換する。World/GPUや保存先を操作しない。
	class MeshOptimizer
	{
	public:
		// 法線・接線生成後に実行する。recipeの確定は取込側が担当する。
		[[nodiscard]] static MeshOptimizationResult Optimize(const MeshAsset& source, MeshOptimizationSettings settings = {});
	};
}
