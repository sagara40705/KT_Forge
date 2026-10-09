#pragma once
#include <cstdint>
#include <limits>

namespace KT::Renderer
{
	// ビューの非所有識別子。親画像のhandleとは異なるview配列のindex。
	struct GraphViewHandle
	{
		std::uint64_t graphid = 0;
		std::uint32_t index = (std::numeric_limits<std::uint32_t>::max)();
		// 未実装: 形式だけ検査。別Graph/破棄済みGraphの参照を許可しない。
		bool IsValid() const noexcept;
	};
}
