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
	//
	void GraphExecutionContext::ClearColor(GraphViewHandle target, const std::array<float, 4>& color)
	{
		// 渡されたtargetが、このパスでRenderTargetとして宣言されているかを検査する
		const auto& viewDesc = RequireDeclaredView(target, GraphResourceUsage::RenderTarget);
		// bindingがColorTargetViewであることを検査する
		auto* colorView = std::get_if<KT::Graphics::ColorTargetView>(&viewDesc.binding);
		if (!colorView)
		{
			throw std::invalid_argument("GraphViewHandleはColorTargetViewではありません");
		}

		// 宣言のaccessを確認する
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

		// commandsにClearRenderTargetを追加する
		commands_.ClearRenderTarget(colorView->GetRtv(), color);
	}

	void GraphExecutionContext::ClearDepth(GraphViewHandle target)
	{
		// 渡されたtargetが、このパスでDepthStencilとして宣言されているかを検査する
		const auto& viewDesc = RequireDeclaredView(target, GraphResourceUsage::DepthStencil);
		// bindingがDepthTargetViewであることを検査する
		auto* depthView = std::get_if<KT::Graphics::DepthTargetView>(&viewDesc.binding);
		if (!depthView)
		{
			throw std::invalid_argument("GraphViewHandleはDepthTargetViewではありません");
		}

		// 宣言のaccessを確認する
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

		// commandsにClearDepthStencilを追加する
		commands_.ClearDepth(*depthView);
	}

	void GraphExecutionContext::DrawIndexed(GraphViewHandle color, GraphViewHandle depth, const KT::Graphics::IndexedDrawPacket& packet)
	{
	}

	// コンストラクタ
	GraphExecutionContext::GraphExecutionContext(
		const RenderGraph& graph, KT::Graphics::CommandContext& commands, const KT::Graphics::ConstantBufferArena& constants, std::size_t passIndex):
		graph_(graph), commands_(commands), constants_(constants), passIndex_(passIndex)
	{
	}

	// 渡されたViewが、このパスで指定された用途として宣言されているか
	const GraphViewDesc& GraphExecutionContext::RequireDeclaredView(GraphViewHandle view, GraphResourceUsage usage) const
	{
		if (!graph_.Contains(view))
		{
			throw std::invalid_argument("GraphViewHandleがこのGraphに属していません");
		}
		if (view.index >= graph_.storage_.views.size())
		{
			throw std::out_of_range("GraphViewHandleのindexが範囲外です");
		}

		// 現在のパスのView宣言を探す
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
