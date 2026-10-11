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
		// 面ごとに頂点を分け、各面へ左上原点の0..1 UVを割り当てる。
		const std::array<KT::Core::Math::Vector3, 8> corners{{{-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f},
			{-.5f, -.5f, .5f}, {.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, .5f}}};
		const std::array<std::array<std::uint32_t, 4>, 6> faces{
			{{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 4, 7, 3}, {1, 2, 6, 5}, {3, 7, 6, 2}, {0, 1, 5, 4}}};
		std::vector<KT::Core::Math::Vector3> positions;
		std::vector<KT::Core::Math::Vector2> texcoords;
		std::vector<std::uint32_t> indices;
		for (const auto& face : faces)
		{
			const auto first = static_cast<std::uint32_t>(positions.size());
			for (const auto corner : face)
			{
				positions.push_back(corners[corner]);
			}
			texcoords.insert(texcoords.end(), {{0, 1}, {0, 0}, {1, 0}, {1, 1}});
			indices.insert(indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
		}
		return {CubeAssetId().Value(), "Cube", KT::Content::MeshAsset(std::move(positions), std::move(indices), std::move(texcoords))};
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
