#pragma once
#include <Renderer/Graph/GraphResourceHandle.h>
#include <Renderer/Graph/GraphViewHandle.h>
#include <Renderer/Graph/GraphTextureRange.h>

namespace KT::Renderer
{
	// WriteAllは指定範囲全体の定義契約。部分draw/writeだけでは満たさない。
	// Read/ReadWriteはpass開始時の既存内容を要求する。
	enum class GraphResourceAccess
	{
		Read,
		WriteAll,
		ReadWrite
	};

	// Viewの種類とは別の利用目的。GPU初版でUnspecifiedは拒否。
	// SRVのstage、readonly DSV、UAV ordering、Copy用途は将来明示追加する。
	enum class GraphResourceUsage
	{
		Unspecified,
		RenderTarget,
		DepthStencil,
	};

	// ビュー経由の用途を画像/rangeへ変換する入口。初版GPUでは未対応を拒否。
	struct GraphViewUse
	{
		GraphViewHandle view{};
		GraphResourceAccess access = GraphResourceAccess::Read;
		GraphResourceUsage usage = GraphResourceUsage::Unspecified;
	};

	// ビュー不要の用途を画像/rangeへ直接宣言する入口。初版GPUでは未対応を拒否。
	// Presentの最終要求はImportedTextureDesc.finalStateで扱い、Present自体はGraph外。
	struct GraphResourceUse
	{
		GraphResourceHandle resource{};
		GraphTextureRange range{};
		GraphResourceAccess access = GraphResourceAccess::Read;
		GraphResourceUsage usage = GraphResourceUsage::Unspecified;
	};
}
