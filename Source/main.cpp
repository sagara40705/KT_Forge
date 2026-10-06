#include <iostream>
using namespace std;

#include "Renderer/Graph/RenderGraphTypes.h"
#include "Renderer/Graph/RenderGraph.h"

int main() 
{
	// 練習用RenderGraphの作成
	KT::Renderer::RenderGraph graph(1);

	// リソースの登録
	KT::Renderer::GraphResourceHandle newHandle = graph.RegisterResource("Output");
	if (!graph.Contains(newHandle))
	{
		cout << "Graph does not contain the registered resource." << endl;
		return 1;
	}

	// 練習用Desc
	KT::Renderer::GraphPassDesc desc;
	desc.name = "Output";

	// 画面クリアのUseを追加
	KT::Renderer::GraphResourceUse use;
	use.resource = newHandle;
	use.access = KT::Renderer::GraphResourceAccess::Write;
	
	desc.resources.push_back(use);

	return 0;
}