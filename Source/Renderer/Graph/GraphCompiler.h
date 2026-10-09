#pragma once
#include <Renderer/Graph/GraphStorage.h>
#include <Renderer/Graph/GraphCompiledPlan.h>
#include <span>

namespace KT::Renderer
{
	// Resolver/Validatorで検査した入力から登録順のRT/DEPTH_WRITE遷移とfinalStateを計画。
	// 所属・範囲・内容依存はValidatorで検査済みが前提。未importと用途Unspecifiedはここでも拒否。
	// 全計画をローカルで完成させて返す。失敗時Storageを保持し、GraphはBuildingのまま。
	class GraphCompiler
	{
	public:
		GraphCompiledPlan Compile(const GraphStorage& storage, std::span<const GraphResolvedPass> passes) const;
	};
}
