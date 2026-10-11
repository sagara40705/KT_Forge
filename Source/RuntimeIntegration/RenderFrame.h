#pragma once
#include <Renderer/RenderData.h>
#include <Renderer/MeshRenderCandidate.h>
#include <World/Entity.h>
#include <optional>

namespace KT::RuntimeIntegration
{
	// 描画候補から除外した理由。未割当は通常状態なので診断を生成しない。
	enum class RenderDiagnosticCode
	{
		MissingAsset,
		AssetTypeMismatch,
		InvalidWorldBounds
	};

	// 問題のあるComponent入力または派生値を識別する。
	enum class RenderDiagnosticField
	{
		Mesh,
		Material,
		WorldBounds
	};

	// ログ出力を伴わない診断値。元Entityは当該Snapshotの識別だけに使用する。
	struct RenderDiagnostic
	{
		KT::World::Entity source;
		RenderDiagnosticField field;
		RenderDiagnosticCode code;
		std::optional<KT::Content::AssetId> asset;
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
	};
}
