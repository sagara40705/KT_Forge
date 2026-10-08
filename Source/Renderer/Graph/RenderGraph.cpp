#include "RenderGraph.h"
#include <cstddef>
#include <utility>

namespace KT::Renderer
{
	// GraphResourceHandleの検査(IsValid・graphidの一致・indexの範囲)
	bool RenderGraph::Contains(GraphResourceHandle handle) const
	{
		if (!handle.IsValid()) return false;
		if (handle.graphid != graphid_) return false;
		if (handle.index >= resources_.size()) return false;
		return true;
	}

	// リソースを登録し、GraphResourceHandleを返す
	GraphResourceHandle RenderGraph::RegisterResource(std::string name)
	{
		// 登録数が最大に達していないか
		if (resources_.size() >= (std::numeric_limits<std::uint32_t>::max)())
		{
			throw std::runtime_error("リソースの登録数が最大に達しました。");
		}

		// 追加前のsizeを、新しいリソースのindexとして覚える
		std::uint32_t index = static_cast<std::uint32_t>(resources_.size());

		ResourceRecord record{};
		record.name = name;

		// リソースを登録
		resources_.push_back(std::move(record));

		// GraphResourceHandleを作成して返す
		GraphResourceHandle handle;
		handle.graphid = graphid_;
		handle.index = index;

		return handle;
	}

	// パスを追加する
	void RenderGraph::AddPass(GraphPassDesc desc, GraphRecordFn record)
	{
		if (desc.name.empty())
		{
			throw std::invalid_argument("パス名が空です。");
		}
		if (desc.resources.empty())
		{
			throw std::invalid_argument("パスに登録されたリソースが空です。");
		}
		if (!record)
		{
			throw std::invalid_argument("パスに登録された記録関数が空です。");
		}

		// 同じリソースの重複指定を拒否する
		std::vector<bool> checkedIndex(resources_.size(), false);

		for (const auto& use : desc.resources)
		{
			if (!Contains(use.resource))
			{
				throw std::invalid_argument("パスに登録されたリソースが無効です。");
			}
			if (use.access != GraphResourceAccess::Read &&
				use.access != GraphResourceAccess::WriteAll &&
				use.access != GraphResourceAccess::ReadWrite)
			{
				throw std::invalid_argument("パスに登録されたリソースのアクセス種別が無効です。");
			}
			if (use.usage != GraphResourceUsage::Unspecified &&
				use.usage != GraphResourceUsage::RenderTarget)
			{
				throw std::invalid_argument("パスに登録されたリソースの用途が無効です。");
			}

			// 重複チェック
			const auto index = use.resource.index;
			if (checkedIndex[index])
			{
				throw std::invalid_argument("パスに登録されたリソースが重複しています。");
			}
			checkedIndex[index] = true;
		}

		// PassRecordを作成してpasses_に追加する
		PassRecord recordEntry{};
		recordEntry.desc = std::move(desc);
		recordEntry.record = std::move(record);
		passes_.push_back(std::move(recordEntry));
	}

	// Graphの検査
	void RenderGraph::Validate() const
	{
		//登録リソース数と同じ長さの「内容が定義済みか」の配列を、全部falseで作る
		const auto resourceCount = resources_.size();
		std::vector<bool> resourceDefined(resourceCount, false);

		//resources_のうち、importedTextureがあるものはcontentsDefinedをresourceDefinedにコピーする
		for (std::size_t index = 0; index < resources_.size(); ++index)
		{
			if (resources_[index].importedTexture.has_value())
			{
				resourceDefined[index] = resources_[index].importedTexture->contentsDefined;
			}
		}

		//passes_を登録順に調べる

		//各パスのresourcesを調べ、
		//ReadかReadWriteなのに対応する値がfalseならstd::runtime_errorをthrowする
		//WriteAllかReadWriteなら対応する値をtrueにする
		for (const auto& pass : passes_)
		{
			const auto& desc = pass.desc;

			for (const auto& use : desc.resources)
			{
				const auto index = use.resource.index;
				if (index >= resourceCount)
				{
					throw std::runtime_error("パスに登録されたリソースのインデックスが無効です。");
				}
				if (use.access == GraphResourceAccess::Read || use.access == GraphResourceAccess::ReadWrite)
				{
					if (!resourceDefined[index])
					{
						throw std::runtime_error("パス '" + desc.name + "' で読み込まれるリソース '" + resources_[index].name + "' が未定義です。");
					}
				}
				if (use.access == GraphResourceAccess::WriteAll || use.access == GraphResourceAccess::ReadWrite)
				{
					resourceDefined[index] = true;
				}
			}
		}
	}

	GraphResourceHandle RenderGraph::ImportTexture(GraphImportedTextureDesc desc)
	{
		if (desc.name.empty())
		{
			throw std::invalid_argument("インポートするテクスチャの名前が空です。");
		}
		if (!desc.resource)
		{
			throw std::invalid_argument("インポートするテクスチャのリソースがnullptrです。");
		}
		if (desc.rtv.ptr == 0)
		{
			throw std::invalid_argument("インポートするテクスチャのRTVが無効です。");
		}
		if (resources_.size() >= ((std::numeric_limits<std::uint32_t>::max)()))
		{
			throw std::overflow_error("リソースの登録数が最大です。");
		}

		//　同じ画像の二重登録を拒否する
		for (const auto& record : resources_)
		{
			if (record.importedTexture.has_value() && record.importedTexture->resource == desc.resource)
			{
				throw std::invalid_argument("同じ画像の二重登録はできません。");
			}
		}

		//追加前のresources_.size()をuint32_tへ変換し、indexとして保存する
		const auto index = static_cast<std::uint32_t>(resources_.size());

		// ResourceRecordを作成してresources_に追加する
		ResourceRecord record{};
		record.name = desc.name;
		record.importedTexture = std::move(desc);
		resources_.push_back(std::move(record));

		// GraphResourceHandleを作成して返す
		GraphResourceHandle handle;
		handle.graphid = graphid_;
		handle.index = index;

		return handle;
	}
}




