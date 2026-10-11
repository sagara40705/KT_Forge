#pragma once
#include <Content/ModelImporter.h>
#include <World/Scene/SceneAsset.h>

namespace KT::RuntimeIntegration
{
	// 名前変更や移動を推測せず、旧key→新keyを制作側が明示する。
	struct ModelReimportOptions
	{
		std::map<std::string, std::string> nodeKeys;
		// 未対応の削除と追加が同時にある場合、新しいノードと明示して進める。
		bool acceptUnmatchedNodes = false;
		// 個体専用資源の読込元。新原本にない指定資源と依存を新bundleへ引き継ぐ。
		std::optional<std::filesystem::path> retainedProject;
		std::vector<KT::Content::AssetId> retainedAssets;
		// 未指定は保存設定を継承。指定は現在の最適化手順への明示移行。
		std::optional<KT::Content::MeshOptimizationSettings> optimizationOverride;
	};

	// 再取込前後の照合key。新ノードのIDは対応表確定前なので保持しない。
	struct ModelNodeTable
	{
		std::map<std::string, KT::Content::ModelNodeId> previous;
		std::vector<std::string> current;
	};

	// 起動可能なSceneと素材bundleの保存結果。Camera/Lightはモデルから自動追加しない。
	struct ModelImportResult
	{
		std::filesystem::path scene;
		std::size_t assetCount = 0;
		std::size_t objectCount = 0;
		std::vector<std::string> diagnostics;
		// Sceneから独立して再利用できるModel原本の保存先と永続ID。
		std::filesystem::path model;
		std::optional<KT::Content::ModelAssetId> modelId;
	};

	// CPU取込結果をScene定義へ接続し、専用の保存先へ一括公開する。
	class ModelImport
	{
	public:
		[[nodiscard]] static KT::World::SceneAsset CreateScene(
			const KT::Content::ImportedModel& model, const std::map<std::string, KT::World::ObjectUuid>& previousIds = {});
		// 新規先だけを許す。全資源を一時directoryへ保存・読込検証してからrenameする。
		[[nodiscard]] static ModelImportResult Import(
			const std::filesystem::path& source, const std::filesystem::path& destination, KT::Content::ModelImportOptions options = {});
		// 前回bundleを残して新規先へ再取込する。同じkeyの素材/ノードUUIDと設定を維持する。
		[[nodiscard]] static ModelImportResult Reimport(
			const std::filesystem::path& previous, const std::filesystem::path& destination, ModelReimportOptions correspondence = {});
		// 保存やWorld生成をせず、変更後の入力を読み取って対応表編集に必要なkeyを列挙する。
		[[nodiscard]] static ModelNodeTable InspectReimport(const std::filesystem::path& previous);
	};
}
