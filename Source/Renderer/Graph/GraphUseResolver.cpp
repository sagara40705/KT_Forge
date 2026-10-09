#include <Renderer/Graph/GraphUseResolver.h>
#include <Renderer/Graph/GraphValidator.h>

namespace KT::Renderer
{
	// View経由の使用を、親画像への使用に変換する
	std::vector<GraphResolvedPass> GraphUseResolver::Resolve(const GraphStorage& storage) const
	{
		std::vector<GraphResolvedPass> resolvedPasses{};
		GraphValidator validator;

		for (std::size_t index = 0; index < storage.passes.size(); ++index)
		{
			//各パスを検査する
			
			validator.ValidatePass(storage.passes[index].desc, storage);

			// GraphResolvedPassを作る
			GraphResolvedPass resolvedPass{};
			resolvedPass.passIndex = index;

			//パスのdesc.viewsをループする
			for (const auto& viewUse : storage.passes[index].desc.views)
			{
				// use.view.indexを使ってstorage.viewsからGraphViewRecordを取得し、そのdescを参照します
				const GraphViewDesc& viewDesc = storage.views[viewUse.view.index].desc;

				// GraphNormalizedUseを作る
				GraphNormalizedUse normalizedUse{};
				normalizedUse.resource = viewDesc.resource; // 親画像のハンドルを
				normalizedUse.range = viewDesc.range; // 親画像の範囲を
				normalizedUse.access = viewUse.access; // アクセス権を
				normalizedUse.usage = viewUse.usage; // 使用用途を
				resolvedPass.uses.push_back(normalizedUse);
			}

			//desc.resourcesもループする
			for (const auto& resourceUse : storage.passes[index].desc.resources)
			{
				// GraphNormalizedUseを作る
				GraphNormalizedUse normalizedResourceUse{};
				normalizedResourceUse.resource = resourceUse.resource; // 親画像のハンドルを
				normalizedResourceUse.range = resourceUse.range; // 親画像の範囲を
				normalizedResourceUse.access = resourceUse.access; // アクセス権を
				normalizedResourceUse.usage = resourceUse.usage; // 使用用途を
				resolvedPass.uses.push_back(normalizedResourceUse);
			}

			resolvedPasses.push_back(std::move(resolvedPass));
		}

		return resolvedPasses;
	}
}
