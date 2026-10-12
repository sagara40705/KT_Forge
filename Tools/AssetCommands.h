#pragma once
#include <optional>

namespace KT::Tools
{
	// 制作コマンドだけを処理する。未該当ならnulloptを返し、Window/GPUを生成しない。
	[[nodiscard]] std::optional<int> RunAssetCommand(int argc, wchar_t* argv[]);
}
