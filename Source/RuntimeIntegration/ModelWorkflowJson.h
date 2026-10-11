#pragma once
#include <RuntimeIntegration/ModelImport.h>
#include <RuntimeIntegration/ModelReconciler.h>

namespace KT::RuntimeIntegration
{
	// 制作用のJSONだけを扱い、Content/Worldの公開型へJSONライブラリを持ち込まない。
	class ModelWorkflowJson
	{
	public:
		[[nodiscard]] static ModelReimportOptions ReadCorrespondence(const std::filesystem::path& path);
		[[nodiscard]] static ModelReconcileSelection ReadSelection(const std::filesystem::path& path);
		// 旧key/NodeIDと変更後keyを含む編集用対応表を安全保存する。
		static void SaveNodeTable(const std::filesystem::path& path, const ModelNodeTable& table);
	};

	// 現在のMeshRendererとModelリンクから、制作snapshotへ引き継ぐ資源IDを集める。
	// 過去原本のrenderer参照は含めない。任意ComponentのAsset参照は呼出側で追加する。
	[[nodiscard]] std::vector<KT::Content::AssetId> CollectModelSceneAssets(
		const KT::World::SceneAsset& scene, const KT::World::ComponentRegistry& components);
}
