#pragma once
#include <Renderer/Graph/GraphViewHandle.h>
#include <array>

namespace KT::Renderer
{
	class RenderGraph;
	// 未実装: view全域のWriteAll/RenderTargetと値捕捉Clear callbackを登録。
	// finite色/宣言/画像適合を検査。旧ResourceHandle引数版は退避済み。
	void AddClearPass(RenderGraph& graph, GraphViewHandle target, std::array<float, 4> color);
}
