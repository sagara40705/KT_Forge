#pragma once
#include <RuntimeIntegration/RenderFrame.h>
#include <Content/AssetCatalog.h>
#include <World/Scene/SceneUpdateContext.h>

namespace KT::RuntimeIntegration
{
	// 完成CPU contextからRenderer用の値をコピーする。他Systemの実行やWorldの再読取は行わない。
	class RenderExtractionSystem
	{
	public:
		// 固定済みCatalogで解決し、欠落は診断として返す。Cameraは候補生成の条件にしない。
		RenderFrame Extract(const KT::World::SceneUpdateContext& context, const KT::Content::AssetCatalog& assets) const;
	};
}
