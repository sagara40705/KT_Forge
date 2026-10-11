#pragma once
#include <Content/AssetDocument.h>
#include <Core/Math/Quaternion.h>
#include <filesystem>
#include <map>

namespace KT::Content
{
	// Assimpの統一座標をエンジンへ変換する。Scaleは位置とノード移動量に適用する。
	struct ModelImportOptions
	{
		float scale = 1;
		bool flipZ = true;
		bool flipV = true;
		MeshNormalMode normals = MeshNormalMode::Preserve;
		bool generateTangents = true;
	};

	// ノードのCPU定義。親/meshはImportedModelの配列位置であり、Entityではない。
	struct ImportedModelNode
	{
		// 再取込の照合に使う、名前と同名兄弟の番号から作るパス。
		std::string key;
		std::string name;
		std::optional<std::size_t> parent;
		std::optional<std::size_t> mesh;
		KT::Core::Math::Vector3 position{};
		KT::Core::Math::Quaternion rotation{};
		KT::Core::Math::Vector3 scale{1, 1, 1};
	};

	// Assimpから独立した取込結果。Importer破棄後もすべての値を所有する。
	struct ImportedModel
	{
		std::vector<AssetDocument> assets;
		// assetsと同じ順の照合key。Meshはノード内の構成、Materialは名前で対応する。
		std::vector<std::string> assetKeys;
		// 取込の補助情報。保存・配置には確定したModelDocumentを使用する。
		std::vector<ImportedModelNode> nodes;
		// nodes.meshが参照するAsset UUIDと、名前付きMaterial割当。
		std::vector<MeshAssetId> meshes;
		std::vector<std::map<std::string, std::optional<MaterialAssetId>>> materials;
		// 保存先相対配置と画像byte列。外部画像と埋込画像を同じ形で保持する。
		std::map<std::string, std::vector<std::uint8_t>> images;
		std::vector<std::string> diagnostics;
		// 取込補助配列から確定した、assets内の唯一のModel原本を返す。
		[[nodiscard]] const AssetDocument& ModelDocument() const;
	};

	// 外部モデルを静的Mesh・Unlit設定・階層へ変換する。World/GPUを生成しない。
	class ModelImporter
	{
	public:
		// sourceとそのディレクトリ内の依存を読む。previousIdsは明示的な再取込の対応表。
		[[nodiscard]] static ImportedModel Load(
			const std::filesystem::path& source, ModelImportOptions options = {}, const std::map<std::string, AssetId>& previousIds = {});
	};
}
