#pragma once
#include <Renderer/Graph/GraphViewHandle.h>

namespace KT::Renderer
{
	class RenderGraph;
	class PreparedRenderFrame;
	// PreparedFrameを値捕捉し、各Drawのpacketをcallback中に記録する。
	// 初版はClearを別passでWriteAll、Opaqueのcolor/depthをReadWriteとする。
	// 内容の前提：この組込みpassは先行ClearとReadWriteを使い、部分描画で全域を定義したと扱わない。
	// 借用寿命：Mesh・Material・arenaは外部がFenceまで保持する。
	// packetの非0描画寸法はPreparedFrameから設定し、color/depth両方との一致を検査する。
	// Drawがある時だけ同じFrameの定数slice・PSOを検査する。binding spanは即時記録中だけ借用する。
	// 空draw一覧は未使用constants/PSOの検査を要求しない。宣言画像/Viewの通常検証は行う。
	void AddOpaquePass(RenderGraph& graph, GraphViewHandle color, GraphViewHandle depth,
		const PreparedRenderFrame& prepared);
}
