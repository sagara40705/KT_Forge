#include <Renderer/Graph/GraphValidator.h>
#include <Graphics/GraphicsValidation.h>
#include <stdexcept>
#include <variant>
#include <limits>
#include <vector>

namespace KT::Renderer
{
	// Viewの検査
	void GraphValidator::ValidateView(const GraphViewDesc& desc, const GraphStorage& storage) const
	{
		if (desc.name.empty())
		{
			throw std::invalid_argument("Viewの名前が空です");
		}
		if (!desc.resource.IsValid())
		{
			throw std::invalid_argument("ViewのGraphResourceHandleが無効です");
		}
		if (desc.resource.graphid != storage.graphid)
		{
			throw std::invalid_argument("ViewのGraphResourceHandleがこのGraphに属していません");
		}
		if (desc.resource.index >= storage.resources.size())
		{
			throw std::out_of_range("ViewのGraphResourceHandleのindexが範囲外です");
		}

		// 親画像はimport済みでなければならない。handle検査後に配列へアクセスする。
		const auto& parent = storage.resources[desc.resource.index];
		if (!parent.importedTexture.has_value())
		{
			throw std::invalid_argument("Viewの親画像がImportTextureで登録されていません");
		}
		auto* resource = parent.importedTexture->resource;
		if (resource == nullptr)
		{
			throw std::invalid_argument("Viewの親import画像がnullptrです");
		}

		// 初版は1Mip/1slice/1planeの全範囲だけを受理する。
		const auto& range = desc.range;
		if (range.firstMip != 0 || range.mipCount != 1 ||
			range.firstSlice != 0 || range.sliceCount != 1 ||
			range.firstPlane != 0 || range.planeCount != 1)
		{
			throw std::invalid_argument("Viewのrangeは各先頭0・個数1の全範囲だけ対応しています");
		}

		const auto image = resource->GetDesc();
		if (image.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		{
			throw std::invalid_argument("Viewの親画像はTexture2Dである必要があります");
		}
		if (image.Width == 0 || image.Height == 0 ||
			image.Width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
			image.Height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
		{
			throw std::invalid_argument("Viewの親画像の幅・高さがTexture2Dの対応範囲外です");
		}
		if (image.MipLevels != 1 || image.DepthOrArraySize != 1 ||
			image.SampleDesc.Count != 1 || image.SampleDesc.Quality != 0)
		{
			throw std::invalid_argument("Viewの親画像は1Mip・1slice・sample count1/quality0だけ対応しています");
		}

		if (desc.binding.valueless_by_exception())
		{
			throw std::invalid_argument("Viewのbindingが例外により値を失っています");
		}
		if (std::holds_alternative<std::monostate>(desc.binding))
		{
			throw std::invalid_argument("Viewのbindingが未設定です");
		}

		// 借用viewの値と親画像を照合する。descriptorの中身や寿命は証明しない。
		if (const auto* color = std::get_if<KT::Graphics::ColorTargetView>(&desc.binding))
		{
			if (color->GetResource() != resource)
			{
				throw std::invalid_argument("Color Viewの画像が親import画像と一致しません");
			}
			if (color->GetWidth() != image.Width || color->GetHeight() != image.Height ||
				color->GetFormat() != image.Format)
			{
				throw std::invalid_argument("Color Viewの幅・高さ・formatが親画像と一致しません");
			}
			if (color->GetRtv().ptr == 0)
			{
				throw std::invalid_argument("Color ViewのRTVが無効です");
			}
			if (!KT::Graphics::IsColorTargetFormat(image.Format) ||
				(image.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == 0)
			{
				throw std::invalid_argument("Color Viewの親画像は対応color formatとALLOW_RENDER_TARGETが必要です");
			}
		}
		else if (const auto* depth = std::get_if<KT::Graphics::DepthTargetView>(&desc.binding))
		{
			if (depth->GetResource() != resource)
			{
				throw std::invalid_argument("Depth Viewの画像が親import画像と一致しません");
			}
			if (depth->GetWidth() != image.Width || depth->GetHeight() != image.Height ||
				depth->GetFormat() != image.Format)
			{
				throw std::invalid_argument("Depth Viewの幅・高さ・formatが親画像と一致しません");
			}
			if (depth->GetDsv().ptr == 0)
			{
				throw std::invalid_argument("Depth ViewのDSVが無効です");
			}
			if (image.Format != DXGI_FORMAT_D32_FLOAT ||
				(image.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) == 0)
			{
				throw std::invalid_argument("Depth Viewの親画像はD32_FLOATとALLOW_DEPTH_STENCILが必要です");
			}
		}
		else
		{
			throw std::invalid_argument("Viewのbinding種別に対応していません");
		}
		// 記録先Deviceの同一性はRecordの責務。storageは変更しない。
	}

	namespace
	{
		// 使用画像のHandleを検査する。配列へのアクセスより先に所属と範囲を確認する。
		void CheckResource(GraphResourceHandle handle, const GraphStorage& storage)
		{
			if (!handle.IsValid() || handle.graphid != storage.graphid)
			{
				throw std::invalid_argument("使用画像のHandleが無効、または別Graphに属しています");
			}
			if (handle.index >= storage.resources.size())
			{
				throw std::out_of_range("使用画像のindexが登録範囲外です");
			}
		}

		// 初版は1Mip/1slice/1planeの全範囲だけを受理する。
		void CheckRange(const GraphTextureRange& range)
		{
			if (range.firstMip != 0 || range.mipCount != 1 ||
				range.firstSlice != 0 || range.sliceCount != 1 ||
				range.firstPlane != 0 || range.planeCount != 1)
			{
				throw std::invalid_argument("使用範囲は各先頭0・個数1だけ対応しています");
			}
		}

		// access/usageの列挙値を確認してから、初版で対応する組合せを検査する。
		void CheckAccessUsage(GraphResourceAccess access, GraphResourceUsage usage)
		{
			if (access != GraphResourceAccess::Read && access != GraphResourceAccess::WriteAll &&
				access != GraphResourceAccess::ReadWrite)
			{
				throw std::invalid_argument("画像accessの列挙値が不正です");
			}
			if (usage != GraphResourceUsage::Unspecified && usage != GraphResourceUsage::RenderTarget &&
				usage != GraphResourceUsage::DepthStencil)
			{
				throw std::invalid_argument("画像usageの列挙値に対応していません");
			}
			if (usage != GraphResourceUsage::Unspecified && access == GraphResourceAccess::Read)
			{
				throw std::invalid_argument("初版のRTV/DSVはWriteAllまたはReadWriteだけ対応しています");
			}
		}

		// 元宣言と正規化Useの画像・範囲・access/usageが完全に一致するかを確認する。
		bool SameUse(const GraphNormalizedUse& firstUse, const GraphNormalizedUse& secondUse)
		{
			return firstUse.resource.graphid == secondUse.resource.graphid &&
				firstUse.resource.index == secondUse.resource.index &&
				firstUse.range.firstMip == secondUse.range.firstMip && firstUse.range.mipCount == secondUse.range.mipCount &&
				firstUse.range.firstSlice == secondUse.range.firstSlice && firstUse.range.sliceCount == secondUse.range.sliceCount &&
				firstUse.range.firstPlane == secondUse.range.firstPlane && firstUse.range.planeCount == secondUse.range.planeCount &&
				firstUse.access == secondUse.access && firstUse.usage == secondUse.usage;
		}

		// Resolverの出力を作る関数ではない。元宣言を照合するための検査用値だけを作る。
		std::vector<GraphNormalizedUse> CheckDeclarations(const GraphPassDesc& desc, const GraphStorage& storage, const GraphValidator& validator)
		{
			if (desc.name.empty())
			{
				throw std::invalid_argument("Passの名前が空です");
			}
			std::vector<GraphNormalizedUse> declarations;

			// Viewの所属・範囲と親import画像を確認し、用途をbindingの種別と照合する。
			for (const auto& use : desc.views)
			{
				if (!use.view.IsValid() || use.view.graphid != storage.graphid)
				{
					throw std::invalid_argument("PassのView Handleが無効、または別Graphです");
				}
				if (use.view.index >= storage.views.size())
				{
					throw std::out_of_range("PassのView indexが登録範囲外です");
				}

				const auto& view = storage.views[use.view.index].desc;
				validator.ValidateView(view, storage);
				validator.ValidateImportedTexture(*storage.resources[view.resource.index].importedTexture, storage);
				CheckAccessUsage(use.access, use.usage);
				if ((std::get_if<KT::Graphics::ColorTargetView>(&view.binding) && use.usage != GraphResourceUsage::RenderTarget) ||
					(std::get_if<KT::Graphics::DepthTargetView>(&view.binding) && use.usage != GraphResourceUsage::DepthStencil))
				{
					throw std::invalid_argument("PassのusageがViewのRTV/DSV種別と一致しません");
				}
				declarations.push_back({view.resource, view.range, use.access, use.usage});
			}

			// 直接Resource宣言はCPU用Unspecifiedだけを受理する。
			for (const auto& use : desc.resources)
			{
				CheckResource(use.resource, storage);
				const auto& parent = storage.resources[use.resource.index];
				if (parent.importedTexture)
				{
					validator.ValidateImportedTexture(*parent.importedTexture, storage);
				}
				CheckRange(use.range);
				CheckAccessUsage(use.access, use.usage);
				if (use.usage != GraphResourceUsage::Unspecified)
				{
					throw std::invalid_argument("直接Resource宣言はCPU用Unspecifiedだけ対応しています");
				}
				declarations.push_back({use.resource, use.range, use.access, use.usage});
			}

			// 全範囲しかない初版では、同じ親画像の使用はすべて重なる。
			// 宣言済みの使用と順に比較し、同usageのRead同士以外は拒否する。
			for (std::size_t useIndex = 0; useIndex < declarations.size(); ++useIndex)
			{
				for (std::size_t previousUseIndex = 0; previousUseIndex < useIndex; ++previousUseIndex)
				{
					if (declarations[useIndex].resource.index == declarations[previousUseIndex].resource.index &&
						(declarations[useIndex].usage != declarations[previousUseIndex].usage ||
						declarations[useIndex].access != GraphResourceAccess::Read ||
						declarations[previousUseIndex].access != GraphResourceAccess::Read))
					{
						throw std::invalid_argument("同じPassの同一画像に重複writeまたは相反usageがあります");
					}
				}
			}
			return declarations;
		}
	}

	// Import画像の検査
	void GraphValidator::ValidateImportedTexture(const GraphImportedTextureDesc& desc, const GraphStorage& storage) const
	{
		if (desc.name.empty() || !desc.resource)
		{
			throw std::invalid_argument("Import画像の名前が空、またはresourceがnullptrです");
		}

		// 登録済みdesc自身だけを除外し、同じGPU画像の二重importを拒否する。
		for (const auto& record : storage.resources)
		{
			if (record.importedTexture && &*record.importedTexture != &desc &&
				record.importedTexture->resource == desc.resource)
			{
				throw std::invalid_argument("同じGPU画像が二重importされています");
			}
		}

		// 初版で対応する画像の種類・寸法・subresource数・sampleを確認する。
		const auto image = desc.resource->GetDesc();
		if (image.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || image.Width == 0 || image.Height == 0 ||
			image.Width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || image.Height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
			image.MipLevels != 1 || image.DepthOrArraySize != 1 || image.SampleDesc.Count != 1 || image.SampleDesc.Quality != 0)
		{
			throw std::invalid_argument("Import画像は対応寸法のTexture2D・1Mip・1slice・sample1/quality0が必要です");
		}

		// format/flagsからColorまたはDepthとして使える画像かを検査する。
		const bool isColorTarget = KT::Graphics::IsColorTargetFormat(image.Format) &&
			(image.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0;
		const bool isDepthTarget = image.Format == DXGI_FORMAT_D32_FLOAT &&
			(image.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0;
		if (!isColorTarget && !isDepthTarget)
		{
			throw std::invalid_argument("Import画像のformat/flagsは初版のRTまたはD32 Depthに対応していません");
		}

		// 初期/最終stateはCOMMONまたは画像種別に対応する書込stateだけを受理する。
		const auto writeState = isColorTarget ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_DEPTH_WRITE;
		if ((desc.initialState != D3D12_RESOURCE_STATE_COMMON && desc.initialState != writeState) ||
			(desc.finalState != D3D12_RESOURCE_STATE_COMMON && desc.finalState != writeState))
		{
			throw std::invalid_argument("Import画像の初期/最終stateが画像種別のCOMMONまたは書込stateと一致しません");
		}
	}

	// Passの元宣言を検査する。
	void GraphValidator::ValidatePass(const GraphPassDesc& desc, const GraphStorage& storage) const
	{
		(void)CheckDeclarations(desc, storage, *this);
	}

	// 全Graphと正規化されたPassを検査する。入力やstorageは変更しない。
	void GraphValidator::Validate(const GraphStorage& storage, std::span<const GraphResolvedPass> passes) const
	{
		// GraphのIDと、Handleのindexで表せる登録数を確認する。
		if (storage.graphid == 0)
		{
			throw std::invalid_argument("GraphStorageのgraphidが無効です");
		}
		if (storage.resources.size() > (std::numeric_limits<std::uint32_t>::max)() ||
			storage.views.size() > (std::numeric_limits<std::uint32_t>::max)())
		{
			throw std::overflow_error("Graphの画像/View登録数がHandleの対応範囲外です");
		}

		// 全登録画像を検査し、import時点の内容定義をローカル配列へコピーする。
		std::vector<bool> contentsDefined(storage.resources.size(), false);
		for (std::size_t resourceIndex = 0; resourceIndex < storage.resources.size(); ++resourceIndex)
		{
			const auto& record = storage.resources[resourceIndex];
			if (record.name.empty())
			{
				throw std::invalid_argument("登録画像の名前が空です");
			}
			if (record.importedTexture)
			{
				// コピーを渡さず、登録されたdesc自身だけを二重importの比較から除外する。
				ValidateImportedTexture(*record.importedTexture, storage);
				contentsDefined[resourceIndex] = record.importedTexture->contentsDefined;
			}
		}

		// 全Viewを検査し、正規化Pass数が元の登録数と一致することを確認する。
		for (const auto& view : storage.views)
		{
			ValidateView(view.desc, storage);
		}
		if (passes.size() != storage.passes.size())
		{
			throw std::invalid_argument("正規化Pass数が登録Pass数と一致しません");
		}

		for (std::size_t passIndex = 0; passIndex < storage.passes.size(); ++passIndex)
		{
			// 記録callbackと元宣言を検査してから、正規化Passの順序を照合する。
			const auto& record = storage.passes[passIndex];
			if (!record.record)
			{
				throw std::invalid_argument("Passの記録callbackが空です");
			}
			const auto declarations = CheckDeclarations(record.desc, storage, *this);
			const auto& resolvedPass = passes[passIndex];
			if (resolvedPass.passIndex != passIndex)
			{
				throw std::invalid_argument("正規化Pass indexが宣言順と一致しません");
			}

			// 正規化Useを検査し、元宣言にない使用や余分な重複がないか確認する。
			for (const auto& use : resolvedPass.uses)
			{
				CheckResource(use.resource, storage);
				CheckRange(use.range);
				CheckAccessUsage(use.access, use.usage);

				std::size_t declarationCount = 0;
				std::size_t resolvedUseCount = 0;
				for (const auto& declared : declarations)
				{
					if (SameUse(use, declared))
					{
						++declarationCount;
					}
				}
				for (const auto& other : resolvedPass.uses)
				{
					if (SameUse(use, other))
					{
						++resolvedUseCount;
					}
				}
				if (declarationCount == 0 || resolvedUseCount > declarationCount)
				{
					throw std::invalid_argument("正規化Useに元宣言にない使用または余分な重複があります");
				}
			}

			// 元宣言の各使用が、正規化結果から欠落していないか確認する。
			for (const auto& declared : declarations)
			{
				bool foundMatchingUse = false;
				for (const auto& use : resolvedPass.uses)
				{
					if (SameUse(declared, use))
					{
						foundMatchingUse = true;
					}
				}
				if (!foundMatchingUse)
				{
					throw std::invalid_argument("正規化Useから元宣言の使用が欠落しています");
				}
			}

			// 同passの出力を反映する前に、すべての読み前提を確認する。
			for (const auto& use : resolvedPass.uses)
			{
				if (use.access != GraphResourceAccess::WriteAll && !contentsDefined[use.resource.index])
				{
					throw std::invalid_argument("Pass開始時にRead/ReadWrite画像の内容が未定義です");
				}
			}

			// 読み前提の検査後、WriteAllの画像を内容定義済みにする。
			for (const auto& use : resolvedPass.uses)
			{
				if (use.access == GraphResourceAccess::WriteAll)
				{
					contentsDefined[use.resource.index] = true;
				}
			}
		}

		// Graph末尾で内容定義が必要なimport画像を確認する。
		for (std::size_t resourceIndex = 0; resourceIndex < storage.resources.size(); ++resourceIndex)
		{
			if (storage.resources[resourceIndex].importedTexture &&
				storage.resources[resourceIndex].importedTexture->requireDefineAtEnd && !contentsDefined[resourceIndex])
			{
				throw std::invalid_argument("requireDefineAtEnd画像がGraph末尾でも未定義です");
			}
		}
	}

}
