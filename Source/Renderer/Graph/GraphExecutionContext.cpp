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
	// ���̃p�X��WriteAll/RenderTarget�錾���m�F���A�J���[�摜�̑S��Clear���L�^����B
	void GraphExecutionContext::ClearColor(GraphViewHandle target, const std::array<float, 4>& color)
	{
		// �O�����F���݃p�X�̐錾�E�p�r�ƁA�ؗpview�̎�ނ��m�F����B
		const auto& viewDesc = RequireDeclaredView(target, GraphResourceUsage::RenderTarget);
		auto* colorView = std::get_if<KT::Graphics::ColorTargetView>(&viewDesc.binding);
		if (!colorView)
		{
			throw std::invalid_argument("GraphViewHandle��ColorTargetView�ł͂���܂���");
		}

		// access�����F�S��Clear�͔͈͑S�̂̓��e���`���邽�߁AWriteAll��v������B
		for (const auto& declaredView : graph_.storage_.passes[passIndex_].desc.views)
		{
			if (declaredView.view.index == target.index && declaredView.view.graphid == target.graphid)
			{
				if (declaredView.access != GraphResourceAccess::WriteAll)
				{
					throw std::invalid_argument("GraphViewHandle��access��WriteAll�ł͂���܂���");
				}
				break;
			}
		}

		// �L�^�F�錾��access�̌������Graphics�֓n���B
		commands_.ClearRenderTarget(colorView->GetRtv(), color);
	}

	// ���̃p�X��WriteAll/DepthStencil�錾���m�F���A�[�x�̑S��Clear��Graphics�ֈ˗�����B
	void GraphExecutionContext::ClearDepth(GraphViewHandle target)
	{
		// �O�����F���݃p�X�̐錾�E�p�r�ƁA�ؗpview�̎�ނ��m�F����B
		const auto& viewDesc = RequireDeclaredView(target, GraphResourceUsage::DepthStencil);
		auto* depthView = std::get_if<KT::Graphics::DepthTargetView>(&viewDesc.binding);
		if (!depthView)
		{
			throw std::invalid_argument("GraphViewHandle��DepthTargetView�ł͂���܂���");
		}

		// access�����F�S��Clear�͔͈͑S�̂̓��e���`���邽�߁AWriteAll��v������B
		for (const auto& declaredView : graph_.storage_.passes[passIndex_].desc.views)
		{
			if (declaredView.view.index == target.index && declaredView.view.graphid == target.graphid)
			{
				if (declaredView.access != GraphResourceAccess::WriteAll)
				{
					throw std::invalid_argument("GraphViewHandle��access��WriteAll�ł͂���܂���");
				}
				break;
			}
		}

		// �L�^�˗��FDepthTargetView�ł�Graphics�����͖������B
		commands_.ClearDepth(*depthView);
	}

	// TODO: �������BReadWrite�錾�Epacket���@�E����Frame�̒萔���������ADraw���L�^����B
	void GraphExecutionContext::DrawIndexed(GraphViewHandle color, GraphViewHandle depth, const KT::Graphics::IndexedDrawPacket& packet)
	{
	}

	// Graph��������������B�����ς݂̃p�Xindex�Ɠ���Frame�̋L�^����Acallback�������؂��B
	GraphExecutionContext::GraphExecutionContext(
		const RenderGraph& graph, KT::Graphics::CommandContext& commands, const KT::Graphics::ConstantBufferArena& constants, std::size_t passIndex):
		graph_(graph), commands_(commands), constants_(constants), passIndex_(passIndex)
	{
	}

	// view�̏����E�͈͂ƌ��݃p�X�̗p�r�錾���m�F���A�o�^���ꂽdesc��Ԃ��B
	const GraphViewDesc& GraphExecutionContext::RequireDeclaredView(GraphViewHandle view, GraphResourceUsage usage) const
	{
		// �O�����F�o�^�z����Q�Ƃ���O��handle���m�F����B
		if (!graph_.Contains(view))
		{
			throw std::invalid_argument("GraphViewHandle������Graph�ɑ����Ă��܂���");
		}
		if (view.index >= graph_.storage_.views.size())
		{
			throw std::out_of_range("GraphViewHandle��index���͈͊O�ł�");
		}

		// �錾�ƍ��F����Graph��view�ł��A���݃p�X�ɂȂ��g�p�͋����Ȃ��B
		for (const auto& declaredView : graph_.storage_.passes[passIndex_].desc.views)
		{
			if (declaredView.view.index == view.index && declaredView.view.graphid == view.graphid)
			{
				if (declaredView.usage != usage)
				{
					throw std::invalid_argument("GraphViewHandle�̗p�r���錾�ƈ�v���܂���");
				}
				return graph_.storage_.views[view.index].desc;
			}
		}
		
		throw std::invalid_argument("GraphViewHandle�����̃p�X�Ő錾����Ă��܂���");
	}

}
