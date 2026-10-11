#include <BuiltinAssets/StaticMeshAssets.h>

namespace KT::BuiltinAssets
{
	KT::Content::MeshAssetId CubeAssetId()
	{
		static const auto id = KT::Content::MeshAssetId::Parse("a84df672-86d0-4530-9394-9fab7f042851");
		return id;
	}

	KT::Content::MaterialAssetId WhiteMaterialAssetId()
	{
		static const auto id = KT::Content::MaterialAssetId::Parse("d394f597-ccee-43a1-a280-415310b03e6a");
		return id;
	}

	KT::Content::AssetDocument CreateCubeAsset()
	{
		// 原点中心・一辺1mのCubeを、既存の表面向きと同じIndex順で定義する。
		return {CubeAssetId().Value(), "Cube",
			KT::Content::MeshAsset({{-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f}, {-.5f, -.5f, .5f},
									   {.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, .5f}},
				{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5, 3, 7, 6, 3, 6, 2, 0, 1, 5, 0, 5, 4})};
	}

	KT::Content::AssetDocument CreateWhiteMaterialAsset()
	{
		return {WhiteMaterialAssetId().Value(), "WhiteUnlit", KT::Content::MaterialAsset{}};
	}

	void RegisterStaticMeshAssets(KT::Content::AssetCatalog& catalog)
	{
		catalog.Register(CreateCubeAsset());
		catalog.Register(CreateWhiteMaterialAsset());
	}
}
