#pragma once
#include <Content/AssetDocument.h>
#include <Core/Utility/NonCopyable.h>
#include <optional>
#include <unordered_map>

namespace KT::Content
{
	// 未割当と参照切れを区別し、Sceneの設定を変更せずに解決結果を返す。
	enum class AssetReferenceState
	{
		Unassigned,
		Missing,
		TypeMismatch,
		Ready
	};

	// Ready時だけ素材を借用できる。借用は所有Catalogの破棄まで有効。
	template <class T> struct AssetLookup
	{
		AssetReferenceState state = AssetReferenceState::Unassigned;
		const T* asset = nullptr;
	};

	// CPU素材を所有する単一threadの登録表。Scene使用前にFreezeし、差替え・削除は行わない。
	class AssetCatalog : private KT::Core::NonCopyable
	{
	public:
		// 検証済みの素材を所有する。失敗時は既存の登録内容を維持する。
		void Register(AssetDocument document);
		// 起動時の一括登録。全件成功した場合だけ置換し、失敗時は既存登録を保持する。
		void RegisterBatch(std::vector<AssetDocument> documents);
		// 素材実体を借用せず、Freeze前にも登録衝突と種類を照合する。
		[[nodiscard]] std::optional<AssetKind> FindKind(AssetId id) const;
		// 起動中の依存収集用に値だけを返す。素材実体の借用はFreeze後に限る。
		[[nodiscard]] std::optional<TextureAssetId> MaterialTexture(MaterialAssetId id) const;
		[[nodiscard]] AssetLookup<TextureAsset> Resolve(std::optional<TextureAssetId> id) const;
		[[nodiscard]] std::size_t Size() const noexcept;
		void Freeze() noexcept;
		[[nodiscard]] bool IsFrozen() const noexcept;
		[[nodiscard]] AssetLookup<MeshAsset> Resolve(std::optional<MeshAssetId> id) const;
		[[nodiscard]] AssetLookup<MaterialAsset> Resolve(std::optional<MaterialAssetId> id) const;
		[[nodiscard]] AssetLookup<ModelAsset> Resolve(std::optional<ModelAssetId> id) const;

	private:
		// 種類をまたいで同一UUIDの二重登録を拒否する。
		std::unordered_map<AssetId, AssetDocument, AssetIdHash> assets_;
		bool frozen_ = false;
	};
}
