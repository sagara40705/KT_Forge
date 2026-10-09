#pragma once
#include <Renderer/Graph/GraphViewHandle.h>

namespace KT::Renderer
{
	class RenderGraph;
	class PreparedRenderFrame;
	// 未実装: PreparedFrame値snapshotをcallback側に保持し、低レベルpacketを即時記録。
	// 初版はClearを別passでWriteAll、Opaqueのcolor/depthをReadWriteとする。
	// この組込みpassの内容契約であり、一般のDrawが過去内容を必ず要求するとは限らない。
	// GraphStorageはPreparedFrameを知らない。Mesh/Material/arenaは外部がFenceまで保持。
	// 各packetのexpectedWidth/expectedHeightへPreparedRenderFrame::GetWidth/GetHeightを設定。
	// 0寸法は拒否し、GraphExecutionContext/Graphicsがcolor/depth両方との一致を検査する。
	// Drawがある時だけ同一Frameの定数slice/PSOを検査。spanは即時記録中だけ借用。
	// 空draw一覧は未使用constants/PSOの検査を要求しない。宣言画像/Viewの通常検証は行う。
	void AddOpaquePass(RenderGraph& graph, GraphViewHandle color, GraphViewHandle depth,
		const PreparedRenderFrame& prepared);
}
