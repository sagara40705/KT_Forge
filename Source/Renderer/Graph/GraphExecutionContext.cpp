#include "Renderer/Graph/GraphExecutionContext.h"
#include "Renderer/Graph/RenderGraph.h"
#include "Graphics/CommandContext.h"
#include <algorithm>
#include <stdexcept>
#include <cmath>

namespace KT::Renderer
{
	void GraphExecutionContext::ClearColor(GraphResourceHandle resource, const std::array<float, 4>& color)
	{
		// 前検査
		if (graph_.state_ != RenderGraph::State::Recording)
		{
			throw std::logic_error("GraphはRecording状態ではありません。");
		}
		if (passIndex_ >= graph_.passes_.size())
		{
			throw std::logic_error("Graphのパス番号が無効です。");
		}
		if (!graph_.Contains(resource))
		{
			throw std::invalid_argument("Graphに無効なリソースHandleです。");
		}

		// 現在のパスに宣言されているか調べる
		const auto& uses = graph_.passes_[passIndex_].desc.resources;
		const auto it = std::find_if(uses.begin(), uses.end(), [resource](const GraphResourceUse& use) {
			return use.resource.graphid == resource.graphid && use.resource.index == resource.index;
			});
		if (it == uses.end())
		{
			throw std::invalid_argument("現在のパスに宣言されていないリソースです。");
		}
		if (it->usage != GraphResourceUsage::RenderTarget)
		{
			throw std::invalid_argument("現在のパスに宣言されているリソースの用途がレンダーターゲットではありません。");
		}
		if (it->access != GraphResourceAccess::WriteAll && it->access != GraphResourceAccess::ReadWrite)
		{
			throw std::invalid_argument("現在のパスに宣言されているリソースのアクセス種別がWriteAllでもReadWriteでもありません。");
		}

		// 色成分の検査
		for (const auto& c : color)
		{
			if (std::isinf(c) || std::isnan(c))
			{
				throw std::invalid_argument("ClearColorの色成分にNaNまたはInfが含まれています。");
			}
		}

		// Import情報の取得
		const auto& imported = graph_.resources_[resource.index].importedTexture;
		if (!imported.has_value())
		{
			throw std::logic_error("Graphに登録されたリソースがインポートされたテクスチャではありません。");
		}
		if (imported->rtv.ptr == 0)
		{
			throw std::logic_error("Graphに登録されたリソースのRTVが無効です。");
		}
		if (!imported->resource)
		{
			throw std::logic_error("Graphに登録されたリソースのID3D12Resourceがnullptrです。");
		}

		// クリア命令を記録する
		commandContext_.ClearRenderTarget(imported->rtv, color);
	}

	GraphExecutionContext::GraphExecutionContext(
		const RenderGraph& graph,
		KT::Graphics::CommandContext& commandContext,
		std::size_t passIndex)
		: graph_(graph), commandContext_(commandContext), passIndex_(passIndex)
	{
	}
}