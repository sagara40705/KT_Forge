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
	// Clearは現在パスの宣言・用途・WriteAllを検査する。DrawIndexedは未実装。
	// VB/IBは外部の不変UPLOAD/GENERIC_READ契約、定数は同じFrame/epochの契約。
	// TODO: 操作入口のContext健全性確認と、失敗時Invalidateによる送信禁止を実装する。
	class GraphExecutionContext : private KT::Core::NonCopyable
	{
	public:
		// WriteAll宣言のカラー画像を全域Clearする。TODO: finite色の検査。
		void ClearColor(GraphViewHandle target, const std::array<float, 4>& color);
		// WriteAll宣言の深度をClearする。TODO: GraphicsのD32/Reverse-Z clear0窓口。
		void ClearDepth(GraphViewHandle target);
		// TODO: 未実装。packetの非0寸法をcolor/depth両viewと照合し、
		// 同じFrameの定数を使ってDrawを記録する。
		void DrawIndexed(GraphViewHandle color, GraphViewHandle depth, const KT::Graphics::IndexedDrawPacket& packet);
	private:
		friend class RenderGraph;
		// Graphだけが生成し、同じFrameの記録先と現在パスindexを借りる。
		GraphExecutionContext(const RenderGraph& graph, KT::Graphics::CommandContext& commands, const KT::Graphics::ConstantBufferArena& constants, std::size_t passIndex);
		// viewの所属・範囲と現在パスの用途宣言を確認し、登録されたdescを返す。
		const GraphViewDesc& RequireDeclaredView(GraphViewHandle view, GraphResourceUsage usage) const;
	private:
		// 使用宣言の正本。callback中は登録内容を変更しない。
		const RenderGraph& graph_;
		// 同じFrameの命令記録先。
		KT::Graphics::CommandContext& commands_;
		// 同じFrameの定数arena。次epochで借用sliceを使い回さない。
		const KT::Graphics::ConstantBufferArena& constants_;
		// Graphが渡す検査済みのパスindex。
		std::size_t passIndex_;
	};
}
