#pragma once
#include <Content/MeshAsset.h>

namespace KT::Content
{
	// CPU取込用の属性生成。元Meshを変更せず、全計算・検査が成功した結果だけ返す。
	class MeshGeometry
	{
	public:
		// Preserveで法線がなければ既存の頂点共有をSmooth生成する。
		// 接線はUV0がある場合に生成。退化面・退化UV・相殺する接線は明示的に拒否する。
		[[nodiscard]] static MeshAsset Generate(
			const MeshAsset& source, MeshNormalMode mode = MeshNormalMode::Preserve, bool generateTangents = true);
	};
}
