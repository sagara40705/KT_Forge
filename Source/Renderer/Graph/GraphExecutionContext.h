#pragma once
#include "Renderer/Graph/RenderGraphTypes.h"
#include "Core/Utility/NonCopyable.h"
#include <array>
#include <cstddef>

namespace KT::Graphics
{
	// 前方宣言
	class CommandContext;
}
namespace KT::Renderer
{
	// 前方宣言
	class RenderGraph;

	// パスのcallbackへ「そのパスが宣言したリソースに描画命令を記録する窓口」を渡す
	// callbackの呼び出し中だけ有効。参照・ポインタを保存して後で使用しない。
	class GraphExecutionContext : private KT::Core::NonCopyable
	{
	public:
		void ClearColor(GraphResourceHandle resource, const std::array<float, 4>& color);

	private:
		// 所属するGraph
		const RenderGraph& graph_;
		// 命令の記録先
		KT::Graphics::CommandContext& commandContext_;
		// 現在のパスの番号
		std::size_t passIndex_;

	private:
		// GraphExecutionContextはRenderGraphからしか作れない
		friend class RenderGraph;
		GraphExecutionContext(
			const RenderGraph& graph, 
			KT::Graphics::CommandContext& commandContext, 
			std::size_t passIndex);
	};
}