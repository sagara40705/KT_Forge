#pragma once
#include <Content/AssetCatalog.h>

namespace KT::Content
{
	// 定義と依存素材の内容をSHA-256で照合する。時刻や実行時handleを含めない。
	[[nodiscard]] std::string ModelRevision(const AssetCatalog& catalog, ModelAssetId model);
	// version 1のkeyを、Model UUIDを名前空間とする安定したノードIDへ移行する。
	[[nodiscard]] ModelNodeId LegacyModelNodeId(AssetId model, std::string_view key);
}
