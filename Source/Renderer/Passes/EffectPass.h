#pragma once
#include <Renderer/Graph/GraphViewHandle.h>

namespace KT::Renderer
{
	class RenderGraph;
	class EffectPlayer;
	struct RenderView;
	// Opaqueの完成色・深度を使い、エフェクトだけを重ねる。
	void AddEffectPass(RenderGraph& graph, GraphViewHandle color, GraphViewHandle depth, EffectPlayer& effects, const RenderView& view);
}
