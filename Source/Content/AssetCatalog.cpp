#include <Content/AssetCatalog.h>
#include <stdexcept>
#include <utility>

namespace KT::Content
{
	const AssetDocument& AssetCatalog::Document(AssetId id) const
	{
		if (!frozen_)
		{
			throw std::logic_error("素材定義の借用前にAssetCatalogをFreezeしてください。");
		}
		const auto found = assets_.find(id);
		if (found == assets_.end())
		{
			throw std::invalid_argument("照合する素材がCatalogにありません: " + id.ToString());
		}
		return found->second;
	}
	void AssetCatalog::Register(AssetDocument document)
	{
		if (frozen_)
		{
			throw std::logic_error("固定後のAssetCatalogへ素材を登録できません。");
		}
		if (!document.id.IsValid() || document.data.valueless_by_exception())
		{
			throw std::invalid_argument("素材登録のAssetIdまたはCPU素材の値が不正です。");
		}
		if (assets_.contains(document.id))
		{
			throw std::invalid_argument("AssetIdが二重登録されています: " + document.id.ToString());
		}
		if (const auto* mesh = std::get_if<MeshAsset>(&document.data))
		{
			mesh->Validate();
		}
		if (const auto* texture = std::get_if<TextureAsset>(&document.data))
		{
			texture->ValidateLoaded();
		}
		const auto id = document.id;
		if (const auto* model = std::get_if<ModelAsset>(&document.data))
		{
			model->Validate();
		}
		assets_.emplace(id, std::move(document));
	}

	void AssetCatalog::Freeze() noexcept
	{
		frozen_ = true;
	}

	void AssetCatalog::RegisterBatch(std::vector<AssetDocument> documents)
	{
		if (frozen_)
		{
			throw std::logic_error("固定後のAssetCatalogへ素材を一括登録できません。");
		}
		if (documents.empty())
		{
			return;
		}

		// 起動中だけ既存値を複製し、途中失敗で一部の素材だけが登録されることを防ぐ。
		AssetCatalog pending;
		pending.assets_ = assets_;
		for (auto& document : documents)
		{
			pending.Register(std::move(document));
		}
		assets_.swap(pending.assets_);
	}

	std::optional<AssetKind> AssetCatalog::FindKind(AssetId id) const
	{
		const auto found = assets_.find(id);
		if (found == assets_.end())
		{
			return std::nullopt;
		}
		return found->second.Kind();
	}

	std::size_t AssetCatalog::Size() const noexcept
	{
		return assets_.size();
	}

	bool AssetCatalog::IsFrozen() const noexcept
	{
		return frozen_;
	}

	// 種類ごとの公開窓口で共通の存在照合を使い、欠落を例外にしない。
	template <class T, class Id>
	static AssetLookup<T> ResolveAsset(const std::unordered_map<AssetId, AssetDocument, AssetIdHash>& assets, const std::optional<Id>& id)
	{
		if (!id)
		{
			return {};
		}
		const auto found = assets.find(id->Value());
		if (found == assets.end())
		{
			return {AssetReferenceState::Missing, nullptr};
		}
		const auto* asset = std::get_if<T>(&found->second.data);
		return {asset ? AssetReferenceState::Ready : AssetReferenceState::TypeMismatch, asset};
	}

	AssetLookup<ModelAsset> AssetCatalog::Resolve(std::optional<ModelAssetId> id) const
	{
		if (!frozen_)
		{
			throw std::logic_error("素材参照の解決前にAssetCatalogをFreezeしてください。");
		}
		return ResolveAsset<ModelAsset>(assets_, id);
	}

	AssetLookup<MeshAsset> AssetCatalog::Resolve(std::optional<MeshAssetId> id) const
	{
		if (!frozen_)
		{
			throw std::logic_error("素材参照の解決前にAssetCatalogをFreezeしてください。");
		}
		return ResolveAsset<MeshAsset>(assets_, id);
	}

	AssetLookup<MaterialAsset> AssetCatalog::Resolve(std::optional<MaterialAssetId> id) const
	{
		if (!frozen_)
		{
			throw std::logic_error("素材参照の解決前にAssetCatalogをFreezeしてください。");
		}
		return ResolveAsset<MaterialAsset>(assets_, id);
	}

	std::optional<TextureAssetId> AssetCatalog::MaterialTexture(MaterialAssetId id) const
	{
		const auto found = assets_.find(id.Value());
		if (found == assets_.end())
		{
			return std::nullopt;
		}
		const auto* material = std::get_if<MaterialAsset>(&found->second.data);
		return material ? material->Settings().baseColorTexture : std::nullopt;
	}

	AssetLookup<TextureAsset> AssetCatalog::Resolve(std::optional<TextureAssetId> id) const
	{
		if (!frozen_)
		{
			throw std::logic_error("素材参照の解決前にAssetCatalogをFreezeしてください。");
		}
		return ResolveAsset<TextureAsset>(assets_, id);
	}

}
