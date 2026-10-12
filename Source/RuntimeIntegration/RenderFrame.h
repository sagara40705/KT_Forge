#pragma once
#include <Renderer/RenderData.h>
#include <Renderer/MeshRenderCandidate.h>
#include <World/Entity.h>
#include <optional>
#include <Renderer/Lighting/SceneLighting.h>

namespace KT::RuntimeIntegration
{
	// 描画候補から除外した理由。未割当は通常状態なので診断を生成しない。
	enum class RenderDiagnosticCode
	{
		MissingAsset,
		AssetTypeMismatch,
		InvalidWorldBounds,
		MissingTexcoords,
		UnknownMaterialSlot,
		MissingNormals,
		SingularToonTransform
	};

	// 問題のあるComponent入力または派生値を識別する。
	enum class RenderDiagnosticField
	{
		Mesh,
		Material,
		WorldBounds,
		BaseColorTexture,
		Texcoords,
		MaterialSlot,
		Normals,
		NormalTransform
	};

	// ログ出力を伴わない診断値。元Entityは当該Snapshotの識別だけに使用する。
	struct RenderDiagnostic
	{
		KT::World::Entity source;
		RenderDiagnosticField field;
		RenderDiagnosticCode code;
		std::optional<KT::Content::AssetId> asset;
		// Mesh自体の診断は空。Submesh診断では範囲番号とslot名を保持する。
		std::optional<std::uint32_t> submesh;
		std::string materialSlot;
	};

	// 同じ完成CPU入力に属する候補と診断。素材Catalogは利用完了まで外部所有者が保持する。
	struct RenderFrame
	{
		KT::Renderer::RenderCandidates candidates;
		// candidates.objectsと同じ順序の元Entity。Rendererへ渡さない。
		std::vector<KT::World::Entity> sources;
		std::vector<RenderDiagnostic> diagnostics;
		std::uint64_t sourceWorldId = 0;
		std::uint64_t updateNumber = 0;
		// Camera未指定またはCamera計算失敗なら空。候補自体はCameraから独立する。
		std::optional<KT::Renderer::RenderView> view;
		// activeな主光源を一つだけコピーする。複数指定は曖昧なため拒否する。
		KT::Renderer::SceneLighting lighting;
	};
}
