#pragma once
#include <Renderer/Graph/GraphViewHandle.h>
#include <array>

namespace KT::Renderer
{
	class RenderGraph;
	// TODO: 未実装。view全域のWriteAll/RenderTargetと値捕捉Clear callbackを登録する。
	// finite色・使用宣言・画像との対応を確認する。画像とdescriptorの所有者はFenceまで保持する。
	void AddClearPass(RenderGraph& graph, GraphViewHandle target, std::array<float, 4> color);
}
