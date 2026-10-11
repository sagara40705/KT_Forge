#pragma once
#include <Content/AssetId.h>
#include <World/Scene/SceneAsset.h>
#include <map>

namespace KT::World
{
	// version 1の数値IDを種類別に明示対応する。0は未割当なので表へ登録しない。
	struct LegacyMeshRendererAssets
	{
		std::map<std::uint64_t, KT::Content::MeshAssetId> meshes;
		std::map<std::uint64_t, KT::Content::MaterialAssetId> materials;
	};

	// 原本を変えずにversion 1だけをversion 2へ変換する。対応が不足したら結果を返さない。
	[[nodiscard]] SceneAsset MigrateMeshRendererAssets(const SceneAsset& source, const LegacyMeshRendererAssets& assets);
}
