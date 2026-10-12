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
	// SRVはPixel stageの深度だけ。readonly DSVは深度判定専用、UAV/Copy用途は扱わない。
	enum class GraphResourceUsage
	{
		Unspecified,
		RenderTarget,
		DepthStencil,
		// readonly DSVによる深度判定。書込み・SRV読取を含まない。
		DepthStencilRead,
		// Pixel Shaderの読取専用深度SRV。DSV書込と同時には使用しない。
		PixelShaderRead,
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
