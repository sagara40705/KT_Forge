#pragma once
#include <RuntimeIntegration/ModelReconciler.h>
#include <filesystem>

namespace KT::RuntimeIntegration
{
	// 制作CLIの明示解決設定。Scene/WorldへJSON型を公開しない。
	[[nodiscard]] ModelReconcileOptions ReadModelResolutions(const std::filesystem::path& path);
}
