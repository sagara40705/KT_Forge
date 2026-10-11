#pragma once
#include <Content/AssetCatalog.h>
#include <World/Scene/SceneAsset.h>
#include <World/Scene/SceneComponents.h>
#include <World/Scene/ComponentRegistry.h>

namespace KT::RuntimeIntegration
{
	// 個体全体の配置。モデル内部のTRSは変更せず、専用の親オブジェクトへ設定する。
	struct ModelPlacementOptions
	{
		std::string name = "Model";
		// 既存Scene内の親UUID。nullなら新しいrootとして配置する。
		std::optional<KT::World::ObjectUuid> parent;
		KT::World::LocalTransform transform;
	};

	// 配置個体の識別情報。Modelノードkeyと新規ObjectUuidの対応だけを返す。
	struct ModelPlacementResult
	{
		KT::World::ObjectUuid root;
		// 同じModelを複数配置しても、この対応表のUUIDを共有しない。
		std::map<std::string, KT::World::ObjectUuid> nodes;
	};

	// 不変モデル定義をSceneの所有値へ展開する。World/GPUは生成しない。
	class ModelPlacement
	{
	public:
		// モデル内部の階層だけをScene化する。再取込に限り前回ObjectUuidを引き継ぐ。
		[[nodiscard]] static KT::World::SceneAsset CreateScene(const KT::Content::ModelAsset& model,
			const std::map<std::string, KT::World::ObjectUuid>& previousIds = {});
		// 全依存とSceneを検証後に一括追加する。例外時は既存Sceneを変更しない。
		[[nodiscard]] static ModelPlacementResult Append(KT::World::SceneAsset& scene, const KT::Content::AssetCatalog& catalog,
			KT::Content::ModelAssetId model, const KT::World::ComponentRegistry& components,
			const KT::World::ScriptRegistry& scripts, ModelPlacementOptions options = {});
	};
}
