#pragma once
#include <filesystem>

namespace KT::Sandbox
{
	// 外部efkefcを既存Toon/SkinのSceneで再生し、追従・停止をゲームから操作する。
	void RunEffectDemo(const std::filesystem::path& effect, unsigned tickLimit);
}
