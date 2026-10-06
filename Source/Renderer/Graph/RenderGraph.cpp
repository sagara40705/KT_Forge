#include "RenderGraph.h"
#include <utility>


// GraphResourceHandleの検査(IsValid・graphidの一致・indexの範囲)
bool KT::Renderer::RenderGraph::Contains(GraphResourceHandle handle) const
{
	if (!handle.IsValid()) return false;
	if (handle.graphid != graphid_) return false;
	if (handle.index >= resourceNames_.size()) return false;

	return true;
}

// リソースを登録し、GraphResourceHandleを返す
KT::Renderer::GraphResourceHandle KT::Renderer::RenderGraph::RegisterResource(std::string name)
{
	// 登録数が最大に達していないか
	if (resourceNames_.size() >= std::numeric_limits<std::uint32_t>::max())
	{
		throw std::runtime_error("リソースの登録数が最大に達しました。");
	}

	// 追加前のsizeを、新しいリソースのindexとして覚える
	std::uint32_t index = static_cast<std::uint32_t>(resourceNames_.size());

	// リソース名を登録
	resourceNames_.push_back(std::move(name));

	// GraphResourceHandleを作成して返す
	GraphResourceHandle handle;
	handle.graphid = graphid_;
	handle.index = index;

	return handle;
}

// パスを追加する
void KT::Renderer::RenderGraph::AddPass(GraphPassDesc desc)
{
	if (desc.name.empty())
	{
		throw std::invalid_argument("パス名が空です。");
	}

	for (const auto& use : desc.resources)
	{
		if (!Contains(use.resource))
		{
			throw std::invalid_argument("パスに登録されたリソースが無効です。");
		}
		if (use.access != GraphResourceAccess::Read &&
			use.access != GraphResourceAccess::Write &&
			use.access != GraphResourceAccess::ReadWrite)
		{
			throw std::invalid_argument("パスに登録されたリソースのアクセス種別が無効です。");
		}
	}
	
	passes_.push_back(std::move(desc));
}
