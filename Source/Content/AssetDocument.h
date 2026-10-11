#pragma once
#include <Content/AssetId.h>
#include <Content/MeshAsset.h>
#include <Content/MaterialAsset.h>
#include <Content/TextureAsset.h>
#include <variant>

namespace KT::Content
{
	// ファイル索引と型付き参照の照合に使用するCPU素材の種類。
	enum class AssetKind
	{
		Mesh,
		Material,
		Texture
	};

	// 素材ファイルとCatalog登録で共有するCPU原本。パスは配置であり、このIDと独立する。
	struct AssetDocument
	{
		AssetId id;
		std::string name;
		std::variant<MeshAsset, MaterialAsset, TextureAsset> data;

		// AssetKindとvariantの並びを一致させ、素材の種類を一か所で判定する。
		[[nodiscard]] AssetKind Kind() const
		{
			return static_cast<AssetKind>(data.index());
		}
	};
}
