#include <Renderer/Graph/GraphValidator.h>
#include <Graphics/GraphicsValidation.h>
#include <stdexcept>
#include <variant>

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

}
