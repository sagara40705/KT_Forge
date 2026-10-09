#pragma once
#include <cstdint>
#include <limits>

namespace KT::Renderer
{
	// 画像の非所有識別子。Viewとは別のindex。Graph破棄後は使用不可、IDは再発行しない。
	struct GraphResourceHandle
	{
		// 所属するGraphのID
		std::uint64_t graphid = 0;

		// Graph内でのindex
		std::uint32_t index = (std::numeric_limits<std::uint32_t>::max)();

		// 非0 IDと無効index以外の形式だけを検査する。所属・登録範囲・寿命は別に確認する。
		bool IsValid() const noexcept
		{
			return graphid != 0 && index != (std::numeric_limits<std::uint32_t>::max)();
		}
	};
}
