#pragma once
#include <Content/ProjectAssets.h>
#include <Content/AssetCatalog.h>
#include <World/Scene/SceneJson.h>

namespace KT::RuntimeIntegration
{
	// 保存時のオブジェクトUUIDとフィールドから、欠落した参照を修正できるようにする。
	struct SceneAssetDiagnostic
	{
		KT::World::ObjectUuid object;
		std::string component;
		std::string field;
		KT::Content::AssetId asset;
		KT::Content::AssetKind expected;
		// 未登録ならnullopt、種類違いなら実際の種類を保持する。
		std::optional<KT::Content::AssetKind> actual;
	};

	// 読込完了後のSceneと参照診断。素材実体は呼出側Catalogが所有する。
	struct ProjectSceneLoadResult
	{
		std::unique_ptr<KT::World::Scene> scene;
		std::vector<SceneAssetDiagnostic> diagnostics;
		std::size_t loadedAssetCount = 0;
	};

	// SceneとContentの接続を担当し、両側へ相互依存を持ち込まない。
	class ProjectSceneLoader
	{
	public:
		// 全MeshRendererの参照を収集し、非表示・非Activeの素材も起動時に用意する。
		// 欠落/種類違いは保持して診断し、破損ファイル/重複UUID/Scene生成失敗は例外とする。
		// 全処理成功後だけ未固定Catalogへ一括登録する。使用前のFreezeは呼出側が行う。
		[[nodiscard]] static ProjectSceneLoadResult Load(const KT::Content::ProjectAssets& project, const std::filesystem::path& scenePath,
			KT::Content::AssetCatalog& catalog, const KT::World::ComponentRegistry& components, const KT::World::ScriptRegistry& scripts);
		// 組込みComponentとMissing Script用の簡易窓口。
		[[nodiscard]] static ProjectSceneLoadResult Load(
			const KT::Content::ProjectAssets& project, const std::filesystem::path& scenePath, KT::Content::AssetCatalog& catalog);
	};
}
