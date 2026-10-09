#pragma once
#include <cstdint>

namespace KT::Renderer
{
	// subresourceの範囲。画素矩形ではない。個数0/範囲外/加算overflowは拒否。
	// 初版は1Mip/1slice/1planeの全体だけ。部分範囲は対応済みと扱わず拒否する。
	struct GraphTextureRange
	{
		std::uint32_t firstMip = 0, mipCount = 1;
		std::uint32_t firstSlice = 0, sliceCount = 1;
		std::uint32_t firstPlane = 0, planeCount = 1;
	};
}
