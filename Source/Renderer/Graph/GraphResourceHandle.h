#pragma once
#include <cstdint>
#include <limits>

namespace KT::Renderer
{
	// 画像の非所有識別子。Viewとは別のindex。Graph破棄後は使用不可、IDは再発行しない。
	struct GraphResourceHandle
	{
		std::uint64_t graphid = 0;
		std::uint32_t index = (std::numeric_limits<std::uint32_t>::max)();
		// 未実装: 無効値の形式検査だけ。所属/範囲/寿命の証明ではない。
		bool IsValid() const noexcept;
	};
}
