#pragma once
#include <optional>

namespace KT::Application
{
	// 制作用Model CLIを処理する。該当しない場合はnulloptを返し、Window/GPUを生成しない。
	[[nodiscard]] std::optional<int> RunModelWorkflowCommand(int argc, wchar_t* argv[]);
}
