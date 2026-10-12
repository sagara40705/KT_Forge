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
	class EffectPlayer;
	class UiRenderer;
	struct RenderView;

	// 現在passの画像宣言だけを解決する記録窓口。callback中のみ有効、保存禁止。
	// ClearはWriteAll、DrawはReadWriteの宣言を検査し、Graphicsへ記録を依頼する。
	// VB/IBは外部の不変・転送完了済み契約、定数は同じFrame/epochの契約。
	// 入口でContext健全性を確認する。検査・記録の例外はInvalidateして再送出し、catch後も失敗を保持する。
	class GraphExecutionContext : private KT::Core::NonCopyable
	{
	public:
		// WriteAll宣言のカラー画像を全域Clearする。TODO: finite色の検査。
		void ClearColor(GraphViewHandle target, const std::array<float, 4>& color);

		// WriteAll宣言の深度をClearする。D32/Reverse-Z clear0を使う。
		void ClearDepth(GraphViewHandle target);

		// packetの非0寸法をcolor/depth両viewと照合し、同じFrameの定数を使ってDrawを記録する。
		void DrawIndexed(GraphViewHandle color, GraphViewHandle depth, const KT::Graphics::IndexedDrawPacket& packet);

		// 現在passのRead宣言とtable内の影画像を照合して記録する。
		void DrawToon(GraphViewHandle color, GraphViewHandle depth, GraphViewHandle shadow, const KT::Graphics::IndexedDrawPacket& packet);
		// 先行Clearされた深度のReadWrite宣言を要求する。
		void DrawDepth(GraphViewHandle depth, const KT::Graphics::IndexedDrawPacket& packet);
		// Effect専用窓口。色ReadWriteとreadonly深度Readの宣言を検査する。
		void DrawEffects(GraphViewHandle color, GraphViewHandle depth, EffectPlayer& effects, const RenderView& view);
		// UI専用窓口。先行定義済みの色ReadWriteだけを借用する。
		void DrawUi(GraphViewHandle color, UiRenderer& ui);

	private:
		friend class RenderGraph;

		// Graphだけが生成し、同じFrameの記録先と現在パスindexを借りる。
		GraphExecutionContext(const RenderGraph& graph, KT::Graphics::CommandContext& commands,
			const KT::Graphics::ConstantBufferArena& constants, std::size_t passIndex);

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
