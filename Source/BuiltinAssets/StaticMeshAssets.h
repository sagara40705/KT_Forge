#pragma once
#include <Content/AssetCatalog.h>

namespace KT::BuiltinAssets
{
	// 起動ごとに採番せず、Sceneから永続参照する組込み素材の固定UUID。
	[[nodiscard]] KT::Content::MeshAssetId CubeAssetId();
	[[nodiscard]] KT::Content::MaterialAssetId WhiteMaterialAssetId();
	[[nodiscard]] KT::Content::AssetDocument CreateCubeAsset();
	[[nodiscard]] KT::Content::AssetDocument CreateWhiteMaterialAsset();
	// GPUを作らず、初版の標準CPU素材だけを登録する。Freezeは所有者が行う。
	void RegisterStaticMeshAssets(KT::Content::AssetCatalog& catalog);
}
