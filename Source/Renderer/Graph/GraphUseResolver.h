#pragma once
#include <Renderer/Graph/GraphStorage.h>
#include <Renderer/Graph/GraphCompiledPlan.h>
#include <vector>

namespace KT::Renderer
{
	// 参照を解決する前にhandle所属/範囲を検査。View→同じ親画像/rangeへ写す。
	// readonlyの完全一致重複だけは統合候補。Read+WriteAllを合成して未定義Readを隠さない。
	// 初版の重なる書込/矛盾用途は拒否し、alias viewを独立画像として扱わない。
	class GraphUseResolver
	{
	public:
		// View経由の使用を、親画像への使用に変換する
		std::vector<GraphResolvedPass> Resolve(const GraphStorage& storage) const;
	};
}
