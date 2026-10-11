#pragma once
#include <Content/ModelImporter.h>
#include <World/Scene/SceneAsset.h>

namespace KT::RuntimeIntegration
{
	// 起動可能なSceneと素材bundleの保存結果。Camera/Lightはモデルから自動追加しない。
	struct ModelImportResult
	{
		std::filesystem::path scene;
		std::size_t assetCount = 0;
		std::size_t objectCount = 0;
		std::vector<std::string> diagnostics;
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
		[[nodiscard]] static ModelImportResult Reimport(const std::filesystem::path& previous, const std::filesystem::path& destination);
	};
}
