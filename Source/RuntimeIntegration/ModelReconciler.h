#pragma once
#include <Content/AssetCatalog.h>
#include <World/Scene/ModelInstance.h>
#include <World/Scene/ComponentRegistry.h>

namespace KT::RuntimeIntegration
{
	// 原本側の親変更と個体TRS編集が競合した場合の、明示した解決方針。
	enum class ModelParentResolution
	{
		Ask,
		KeepLocal,
		KeepWorld,
		FollowSource
	};

	// 一つの個体ノードだけに適用する制作時の解決指定。
	struct ModelNodeResolution
	{
		ModelParentResolution parent = ModelParentResolution::Ask;
		// ノードとその現在の子孫を通常Sceneオブジェクトとして保持する。
		bool detach = false;
		// 抑止ノードと抑止中の原本子孫を最新原本から復元する。個体の追加Componentは復元しない。
		bool restore = false;
		// 指定フィールドのoverrideを解除して、新しい原本値へ戻す。
		std::set<std::string> followFields;
	};

	// ObjectUuidは配置root、ModelNodeIdは原本ノードを示す。
	using ModelReconcileOptions = std::map<std::pair<KT::World::ObjectUuid, KT::Content::ModelNodeId>, ModelNodeResolution>;

	// 空集合は制限なし。両方指定した場合はModelと配置rootの両条件を満たす個体だけを反映する。
	struct ModelReconcileSelection
	{
		std::set<KT::Content::ModelAssetId> models;
		std::set<KT::World::ObjectUuid> roots;
	};

	// 自動判断できなかった対象と理由。解決指定を付けてPlanを作り直す。
	struct ModelReconcileConflict
	{
		KT::World::ObjectUuid instance;
		std::optional<KT::Content::ModelNodeId> node;
		std::string reason;
	};

	// 元Sceneと候補を所有し、Plan後の編集や候補の改変による誤反映を防ぐ。
	class ModelReconcilePlan
	{
	public:
		[[nodiscard]] const KT::World::SceneAsset& Preview() const noexcept;
		[[nodiscard]] std::span<const ModelReconcileConflict> Conflicts() const noexcept;

	private:
		friend class ModelReconciler;
		ModelReconcilePlan() = default;
		KT::World::SceneAsset before_;
		KT::World::SceneAsset candidate_;
		std::vector<ModelReconcileConflict> conflicts_;
	};

	// 制作・保存用Sceneの差分反映。Script更新、稼働中World、GPUの差替えは行わない。
	class ModelReconciler
	{
	public:
		// 明示対応を持つ既存配置へリンクを追加する。旧原本Catalogを渡し、現在値は保持する。
		static void Attach(KT::World::SceneAsset& scene, const KT::Content::AssetCatalog& catalog, KT::Content::ModelAssetId model,
			KT::World::ObjectUuid root, const std::map<KT::Content::ModelNodeId, KT::World::ObjectUuid>& objects,
			const KT::World::ComponentRegistry& components, const KT::World::ScriptRegistry& scripts);
		// 同値でも個体編集として固定する。解除・原本復帰はPlanのfollowFieldsで行う。
		static void SetOverride(KT::World::SceneAsset& scene, KT::World::ObjectUuid root, KT::Content::ModelNodeId node, std::string field);
		// 指定個体を三者比較する。非対象の定義を保持し、失敗・競合時にも入力Sceneは変更しない。
		[[nodiscard]] static ModelReconcilePlan Plan(const KT::World::SceneAsset& scene, const KT::Content::AssetCatalog& catalog,
			const KT::World::ComponentRegistry& components, const KT::World::ScriptRegistry& scripts,
			const ModelReconcileOptions& options = {}, const ModelReconcileSelection& selection = {});
		// 未解決競合と元Sceneの変更を拒否し、検証済み候補だけをswapで反映する。
		static void Apply(KT::World::SceneAsset& scene, const ModelReconcilePlan& plan, const KT::World::ComponentRegistry& components,
			const KT::World::ScriptRegistry& scripts);
	};
}
