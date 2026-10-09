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
		// 非0 IDと無効index以外の形式だけを検査する。所属・登録範囲・寿命は別に確認する。
		bool IsValid() const noexcept;
	};
}
