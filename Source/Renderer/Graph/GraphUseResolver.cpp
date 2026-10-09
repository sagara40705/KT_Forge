#include <Renderer/Graph/GraphUseResolver.h>
#include <Renderer/Graph/GraphValidator.h>
#include <utility>

namespace KT::Renderer
{
	// 宣言を検査して、viewと直接画像の使用を親画像・範囲へ正規化する。入力は変更しない。
	std::vector<GraphResolvedPass> GraphUseResolver::Resolve(const GraphStorage& storage) const
	{
		std::vector<GraphResolvedPass> resolvedPasses{};
		GraphValidator validator;

		for (std::size_t index = 0; index < storage.passes.size(); ++index)
		{
			// 前検査：handleの所属・範囲と使用の競合を確認してから参照する。

			validator.ValidatePass(storage.passes[index].desc, storage);

			// パスの準備：登録順のindexを結果にも保持する。
			GraphResolvedPass resolvedPass{};
			resolvedPass.passIndex = index;

			// view使用の登録：親画像・範囲へ写し、access/usageは宣言どおり保持する。
			for (const auto& viewUse : storage.passes[index].desc.views)
			{
				const GraphViewDesc& viewDesc = storage.views[viewUse.view.index].desc;

				GraphNormalizedUse normalizedUse{};
				normalizedUse.resource = viewDesc.resource;
				normalizedUse.range = viewDesc.range;
				normalizedUse.access = viewUse.access;
				normalizedUse.usage = viewUse.usage;
				resolvedPass.uses.push_back(normalizedUse);
			}

			// 直接画像使用の登録：viewの有無にかかわらず、元宣言を独立して追加する。
			for (const auto& resourceUse : storage.passes[index].desc.resources)
			{
				GraphNormalizedUse normalizedResourceUse{};
				normalizedResourceUse.resource = resourceUse.resource;
				normalizedResourceUse.range = resourceUse.range;
				normalizedResourceUse.access = resourceUse.access;
				normalizedResourceUse.usage = resourceUse.usage;
				resolvedPass.uses.push_back(normalizedResourceUse);
			}

			// 結果の登録：完成したパスを1回だけ追加する。
			resolvedPasses.push_back(std::move(resolvedPass));
		}

		return resolvedPasses;
	}
}
