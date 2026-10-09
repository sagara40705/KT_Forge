#pragma once
#include <Renderer/Graph/GraphViewHandle.h>

namespace KT::Renderer
{
	class RenderGraph;
	class PreparedRenderFrame;
	// 未実装: PreparedFrame値snapshotをcallback側に保持し、低レベルpacketを即時記録。
	// color/depthとも先行Clear済みReadWrite。画素部分drawをWriteAllへ昇格しない。
	// GraphStorageはPreparedFrameを知らない。Mesh/Material/arenaは外部がFenceまで保持。
	// 各packetのexpectedWidth/expectedHeightへPreparedRenderFrame::GetWidth/GetHeightを設定。
	// 0寸法は拒否し、GraphExecutionContext/Graphicsがcolor/depth両方との一致を検査する。
	// 同一Frameの定数sliceも検査。spanはcallbackの記録呼出中だけ借用。
	void AddOpaquePass(RenderGraph& graph, GraphViewHandle color, GraphViewHandle depth,
		const PreparedRenderFrame& prepared);
}
