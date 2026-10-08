#pragma once
#include "Renderer/Graph/RenderGraphTypes.h"
#include <array>

namespace KT::Renderer
{
	// 前方宣言
	class RenderGraph;

	// RenderGraphに全域Clearパスを追加する。targetはRenderTargetとして宣言されている必要がある
	void AddClearPass(RenderGraph& graph, GraphResourceHandle target, std::array<float, 4> color);
}