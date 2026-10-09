#pragma once
#include <source_location>
#include <string_view>

namespace KT::Core::Detail
{
	// プログラム内部の前提が崩れた場合の終了処理。
	// 診断の出力に失敗してもabortで停止する。例外による復帰は想定しない。
	[[noreturn]] void AssertFailure(std::string_view expression, std::source_location location) noexcept;
}

// 開発中に「ここでは必ず成り立つはずの条件」を確認するために使う。
// NDEBUGが未定義なら条件を1回評価し、偽なら診断を出して停止する。
// NDEBUGが定義されたReleaseでは条件式ごと無効になるため、初期化やAPI呼出しを入れない。
// 入力不正やファイル読込失敗など、Releaseでも必要な検査は通常の分岐や例外で扱う。
#ifndef NDEBUG
#define KT_ASSERT(condition)                                                                                                               \
	do                                                                                                                                     \
	{                                                                                                                                      \
		if (!static_cast<bool>(condition))                                                                                                 \
		{                                                                                                                                  \
			::KT::Core::Detail::AssertFailure(#condition, ::std::source_location::current());                                              \
		}                                                                                                                                  \
	} while (false)
#else
#define KT_ASSERT(condition)                                                                                                               \
	do                                                                                                                                     \
	{                                                                                                                                      \
	} while (false)
#endif
