#include <Renderer/Graph/GraphExecutionContext.h>
#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Graph/GraphView.h>
#include <Renderer/Graph/GraphResourceUse.h>
#include <Graphics/CommandContext.h>
#include <Graphics/ConstantBufferArena.h>
#include <Graphics/IndexedDrawPacket.h>
#include <Graphics/ColorTargetView.h>
#include <Graphics/DepthTargetView.h>
#include <stdexcept>
#include <variant>

namespace KT::Renderer
{
	// このパスのWriteAll/RenderTarget宣言を確認し、カラー画像の全域Clearを記録する。
	void GraphExecutionContext::ClearColor(GraphViewHandle target, const std::array<float, 4>& color)
	{
		try
		{
			// 前検査：失敗済みのContextでは、宣言検査や追加記録を始めない。
			(void)commands_.GetRecordingList();

			// 前検査：現在パスの宣言・用途と、借用viewの種類を確認する。
			const auto& viewDesc = RequireDeclaredView(target, GraphResourceUsage::RenderTarget);
			auto* colorView = std::get_if<KT::Graphics::ColorTargetView>(&viewDesc.binding);
			if (!colorView)
			{
				throw std::invalid_argument("GraphViewHandleはColorTargetViewではありません");
			}

			// access検査：全域Clearは範囲全体の内容を定義するため、WriteAllを要求する。
			for (const auto& declaredView : graph_.storage_.passes[passIndex_].desc.views)
			{
				if (declaredView.view.index == target.index && declaredView.view.graphid == target.graphid)
				{
					if (declaredView.access != GraphResourceAccess::WriteAll)
					{
						throw std::invalid_argument("GraphViewHandleのaccessがWriteAllではありません");
					}
					break;
				}
			}

			// 記録：宣言とaccessの検査後にGraphicsへ渡す。
			commands_.ClearRenderTarget(colorView->GetRtv(), color);
		}
		catch (...)
		{
			// 失敗保持：callbackが例外をcatchしても、このlistの記録・送信を禁止する。
			commands_.Invalidate();
			throw;
		}
	}

	// このパスのWriteAll/DepthStencil宣言を確認し、深度の全域ClearをGraphicsへ依頼する。
	void GraphExecutionContext::ClearDepth(GraphViewHandle target)
	{
		try
		{
			// 前検査：失敗済みのContextでは、宣言検査や追加記録を始めない。
			(void)commands_.GetRecordingList();

			// 前検査：現在パスの宣言・用途と、借用viewの種類を確認する。
			const auto& viewDesc = RequireDeclaredView(target, GraphResourceUsage::DepthStencil);
			auto* depthView = std::get_if<KT::Graphics::DepthTargetView>(&viewDesc.binding);
			if (!depthView)
			{
				throw std::invalid_argument("GraphViewHandleはDepthTargetViewではありません");
			}

			// access検査：全域Clearは範囲全体の内容を定義するため、WriteAllを要求する。
			for (const auto& declaredView : graph_.storage_.passes[passIndex_].desc.views)
			{
				if (declaredView.view.index == target.index && declaredView.view.graphid == target.graphid)
				{
					if (declaredView.access != GraphResourceAccess::WriteAll)
					{
						throw std::invalid_argument("GraphViewHandleのaccessがWriteAllではありません");
					}
					break;
				}
			}

			// 記録依頼：DepthTargetView版のGraphics窓口は未実装。
			commands_.ClearDepth(*depthView);
		}
		catch (...)
		{
			// 失敗保持：callbackが例外をcatchしても、このlistの記録・送信を禁止する。
			commands_.Invalidate();
			throw;
		}
	}

	// ReadWrite宣言・packet寸法・同じFrameの定数を検査し、Drawを記録する。
	void GraphExecutionContext::DrawIndexed(GraphViewHandle color, GraphViewHandle depth, const KT::Graphics::IndexedDrawPacket& packet)
	{
		try
		{
			// 前検査：失敗済みのContextでは、宣言検査や追加記録を始めない。
			(void)commands_.GetRecordingList();

            // 色と深度の宣言を取得する
            const auto& colorViewDesc = RequireDeclaredView(color, GraphResourceUsage::RenderTarget);
            const auto& depthViewDesc = RequireDeclaredView(depth, GraphResourceUsage::DepthStencil);

            // bindingからViewを取り出す
            auto* colorView = std::get_if<KT::Graphics::ColorTargetView>(&colorViewDesc.binding);
            auto* depthView = std::get_if<KT::Graphics::DepthTargetView>(&depthViewDesc.binding);
            if (!colorView || !depthView)
            {
                throw std::invalid_argument("GraphViewHandleがColorTargetViewまたはDepthTargetViewではありません");
            }

            //両方のaccessがReadWriteか確認する
            for (const auto& declaredView : graph_.storage_.passes[passIndex_].desc.views)
            {
                if (declaredView.view.index == color.index && declaredView.view.graphid == color.graphid)
                {
                    if (declaredView.access != GraphResourceAccess::ReadWrite)
                    {
                        throw std::invalid_argument("Color GraphViewHandleのaccessがReadWriteではありません");
                    }
                }
                if (declaredView.view.index == depth.index && declaredView.view.graphid == depth.graphid)
                {
                    if (declaredView.access != GraphResourceAccess::ReadWrite)
                    {
                        throw std::invalid_argument("Depth GraphViewHandleのaccessがReadWriteではありません");
                    }
                }
            }

            // 描画サイズを確認する
            if (packet.expectedWidth == 0 || packet.expectedHeight == 0)
            {
                throw std::invalid_argument("IndexedDrawPacketのexpectedWidthまたはexpectedHeightが0です");
            }

            // Graphicsへ渡す
            commands_.DrawIndexed(packet, *colorView, *depthView, constants_);
		}
		catch (...)
		{
			// 失敗保持：callbackが例外をcatchしても、このlistの記録・送信を禁止する。
			commands_.Invalidate();
			throw;
		}
	}

	// Graphだけが生成する。検査済みのパスindexと同じFrameの記録先を、callback中だけ借りる。
	GraphExecutionContext::GraphExecutionContext(
		const RenderGraph& graph, KT::Graphics::CommandContext& commands, const KT::Graphics::ConstantBufferArena& constants, std::size_t passIndex):
		graph_(graph), commands_(commands), constants_(constants), passIndex_(passIndex)
	{
	}

	// viewの所属・範囲と現在パスの用途宣言を確認し、登録されたdescを返す。
	const GraphViewDesc& GraphExecutionContext::RequireDeclaredView(GraphViewHandle view, GraphResourceUsage usage) const
	{
		// 前検査：登録配列を参照する前にhandleを確認する。
		if (!graph_.Contains(view))
		{
			throw std::invalid_argument("GraphViewHandleがこのGraphに属していません");
		}
		if (view.index >= graph_.storage_.views.size())
		{
			throw std::out_of_range("GraphViewHandleのindexが範囲外です");
		}

		// 宣言照合：同じGraphのviewでも、現在パスにない使用は許可しない。
		for (const auto& declaredView : graph_.storage_.passes[passIndex_].desc.views)
		{
			if (declaredView.view.index == view.index && declaredView.view.graphid == view.graphid)
			{
				if (declaredView.usage != usage)
				{
					throw std::invalid_argument("GraphViewHandleの用途が宣言と一致しません");
				}
				return graph_.storage_.views[view.index].desc;
			}
		}
		
		throw std::invalid_argument("GraphViewHandleがこのパスで宣言されていません");
	}

}
