#pragma once
#include <Graphics/ConstantBufferArena.h>
#include <d3d12.h>
#include <span>

namespace KT::Graphics
{
	class GraphicsPipelineState;
	class VertexBuffer;
	class IndexBuffer;
	// 即時記録の引数だけ。pipeline/buffer/binding配列を所有しない。
	// spanの参照先は記録呼出が戻るまで保持。GPU実体/定数arenaはFence完了まで保持。
	// Graphがbuffer依存やshaderの全参照を自動検出するpacketではない。
	struct IndexedDrawPacket
	{
		const GraphicsPipelineState* pipeline = nullptr;
		const VertexBuffer* vertices = nullptr;
		const IndexBuffer* indices = nullptr;
		std::span<const RootConstantBinding> bindings{};
		// callerの期待描画寸法。0は不正値で、未指定として許可しない。
		// GraphExecutionContext/Graphicsはcolor/depth両方との一致を命令追加前に検査する。
		UINT expectedWidth = 0;
		UINT expectedHeight = 0;
	};
}
