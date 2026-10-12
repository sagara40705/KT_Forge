#include <Renderer/Graph/GraphExecutionContext.h>
#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Graph/GraphView.h>
#include <Renderer/Graph/GraphResourceUse.h>
#include <Graphics/Commands/CommandContext.h>
#include <Graphics/Buffers/ConstantBufferArena.h>
#include <Graphics/Commands/IndexedDrawPacket.h>
#include <Graphics/Textures/ColorTargetView.h>
#include <Graphics/Textures/DepthTargetView.h>
#include <stdexcept>
#include <Graphics/Textures/ColorDepthTextureTable.h>
#include <variant>
#include <Renderer/Effects/EffectPlayer.h>
#include <Renderer/UI/UiRenderer.h>

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

			// 宣言検査後に借用DSVの全域Clearを記録する。
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

			if (packet.texturePair)
			{
				throw std::invalid_argument("影を読むDrawにはGraphExecutionContext::DrawToonを使用してください。");
			}
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

			// 両方のaccessがReadWriteか確認する
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

	void GraphExecutionContext::DrawDepth(GraphViewHandle depth, const KT::Graphics::IndexedDrawPacket& packet)
	{
		try
		{
			(void)commands_.GetRecordingList();
			const auto& desc = RequireDeclaredView(depth, GraphResourceUsage::DepthStencil);
			const auto* target = std::get_if<KT::Graphics::DepthTargetView>(&desc.binding);
			for (const auto& use : graph_.storage_.passes[passIndex_].desc.views)
			{
				if ((use.view.graphid == depth.graphid && use.view.index == depth.index) && use.access != GraphResourceAccess::ReadWrite)
				{
					throw std::invalid_argument("Shadow Drawの深度には先行Clear後のReadWrite宣言が必要です。");
				}
			}
			if (!target || packet.texturePair)
			{
				throw std::invalid_argument("Shadow Drawの深度viewまたはTexture指定が不正です。");
			}
			commands_.DrawDepth(packet, *target, constants_);
		}
		catch (...)
		{
			commands_.Invalidate();
			throw;
		}
	}

	void GraphExecutionContext::DrawToon(
		GraphViewHandle color, GraphViewHandle depth, GraphViewHandle shadow, const KT::Graphics::IndexedDrawPacket& packet)
	{
		try
		{
			(void)commands_.GetRecordingList();
			const auto& colorDesc = RequireDeclaredView(color, GraphResourceUsage::RenderTarget);
			const auto& depthDesc = RequireDeclaredView(depth, GraphResourceUsage::DepthStencil);
			const auto& shadowDesc = RequireDeclaredView(shadow, GraphResourceUsage::PixelShaderRead);
			const auto* colorTarget = std::get_if<KT::Graphics::ColorTargetView>(&colorDesc.binding);
			const auto* depthTarget = std::get_if<KT::Graphics::DepthTargetView>(&depthDesc.binding);
			const auto* sampled = std::get_if<KT::Graphics::PixelDepthView>(&shadowDesc.binding);
			if (!colorTarget || !depthTarget || !sampled || !packet.texturePair ||
				packet.texturePair->GetDepthResource() != sampled->GetResource())
			{
				throw std::invalid_argument("Toon Drawの影table画像がGraphの読取viewと一致しません。");
			}
			for (const auto& use : graph_.storage_.passes[passIndex_].desc.views)
			{
				if ((((use.view.graphid == color.graphid && use.view.index == color.index) ||
						 (use.view.graphid == depth.graphid && use.view.index == depth.index)) &&
						use.access != GraphResourceAccess::ReadWrite) ||
					((use.view.graphid == shadow.graphid && use.view.index == shadow.index) && use.access != GraphResourceAccess::Read))
				{
					throw std::invalid_argument("Toon Drawには色/深度のReadWriteと影のRead宣言が必要です。");
				}
			}
			commands_.DrawIndexed(packet, *colorTarget, *depthTarget, constants_);
		}
		catch (...)
		{
			commands_.Invalidate();
			throw;
		}
	}

	void GraphExecutionContext::DrawUi(GraphViewHandle color, UiRenderer& ui)
	{
		try
		{
			(void)commands_.GetRecordingList();
			const auto& description = RequireDeclaredView(color, GraphResourceUsage::RenderTarget);
			const auto* target = std::get_if<KT::Graphics::ColorTargetView>(&description.binding);
			if (!target)
			{
				throw std::invalid_argument("UI passには色RTVが必要です。");
			}
			for (const auto& use : graph_.storage_.passes[passIndex_].desc.views)
			{
				if (use.view.index == color.index && use.access != GraphResourceAccess::ReadWrite)
				{
					throw std::invalid_argument("UI passには色ReadWriteの宣言が必要です。");
				}
			}
			ui.Record(commands_, *target);
		}
		catch (...)
		{
			commands_.Invalidate();
			throw;
		}
	}

	void GraphExecutionContext::DrawEffects(GraphViewHandle color, GraphViewHandle depth, EffectPlayer& effects, const RenderView& view)
	{
		try
		{
			(void)commands_.GetRecordingList();
			const auto& colorDesc = RequireDeclaredView(color, GraphResourceUsage::RenderTarget);
			const auto& depthDesc = RequireDeclaredView(depth, GraphResourceUsage::DepthStencilRead);
			const auto* colorTarget = std::get_if<KT::Graphics::ColorTargetView>(&colorDesc.binding);
			const auto* depthTarget = std::get_if<KT::Graphics::DepthTargetView>(&depthDesc.binding);
			if (!colorTarget || !depthTarget || !depthTarget->IsReadOnly())
			{
				throw std::invalid_argument("Effect passには色RTVとreadonly深度DSVが必要です。");
			}
			// 現在passの画像宣言を確認し、上流へ未宣言の出力を渡さない。
			for (const auto& use : graph_.storage_.passes[passIndex_].desc.views)
			{
				if ((use.view.index == color.index && use.access != GraphResourceAccess::ReadWrite) ||
					(use.view.index == depth.index && use.access != GraphResourceAccess::Read))
				{
					throw std::invalid_argument("Effect passには色ReadWriteと深度Readの宣言が必要です。");
				}
			}
			effects.Record(commands_, *colorTarget, *depthTarget, view);
		}
		catch (...)
		{
			commands_.Invalidate();
			throw;
		}
	}

	// Graphだけが生成する。検査済みのパスindexと同じFrameの記録先を、callback中だけ借りる。
	GraphExecutionContext::GraphExecutionContext(const RenderGraph& graph, KT::Graphics::CommandContext& commands,
		const KT::Graphics::ConstantBufferArena& constants, std::size_t passIndex)
		: graph_(graph),
		  commands_(commands),
		  constants_(constants),
		  passIndex_(passIndex)
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
