#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Renderer/Graph/GraphViewHandle.h>
#include <Renderer/Graph/GraphResourceUse.h>
#include <array>
#include <cstddef>

namespace KT::Graphics
{
	class CommandContext;
	class ConstantBufferArena;
	struct IndexedDrawPacket;
}
namespace KT::Renderer
{
	class RenderGraph;
	struct GraphViewDesc;
	// 現在passの画像宣言だけを解決する記録窓口。callback中のみ有効、保存禁止。
	// 未宣言画像、別Graph、用途/access違いを命令追加前に拒否。raw listを公開しない。
	// VB/IBは外部の不変UPLOAD/GENERIC_READ契約、定数は同じFrame/epochの契約。
	// shaderや外部GPU参照をすべて自動検出する機能ではない。
	class GraphExecutionContext : private KT::Core::NonCopyable
	{
	public:
		void ClearColor(GraphViewHandle target, const std::array<float, 4>& color);
		void ClearDepth(GraphViewHandle target); // D32 Reverse-Z clear0。
		// packet.expectedWidth/expectedHeightの0を拒否し、color/depth両viewの寸法と
		// 一致することを命令追加前に検査する。Graphicsも同じサイズ契約を再検査する。
		void DrawIndexed(GraphViewHandle color, GraphViewHandle depth, const KT::Graphics::IndexedDrawPacket& packet);
	private:
		friend class RenderGraph;
		GraphExecutionContext(const RenderGraph& graph, KT::Graphics::CommandContext& commands,
			const KT::Graphics::ConstantBufferArena& constants, std::size_t passIndex);
		const GraphViewDesc& RequireDeclaredView(GraphViewHandle view, GraphResourceUsage usage) const;
		const RenderGraph& graph_;
		KT::Graphics::CommandContext& commands_;
		const KT::Graphics::ConstantBufferArena& constants_;
		std::size_t passIndex_;
	};
}
