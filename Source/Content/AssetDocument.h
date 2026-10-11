#pragma once
#include <Content/AssetId.h>
#include <Content/MeshAsset.h>
#include <Content/MaterialAsset.h>
#include <variant>

namespace KT::Content
{
	// ファイル索引と型付き参照の照合に使用するCPU素材の種類。
	enum class AssetKind
	{
		Mesh,
		Material
	};

	// 素材ファイルとCatalog登録で共有するCPU原本。パスは配置であり、このIDと独立する。
	struct AssetDocument
	{
		AssetId id;
		std::string name;
		std::variant<MeshAsset, MaterialAsset> data;
	};
}
