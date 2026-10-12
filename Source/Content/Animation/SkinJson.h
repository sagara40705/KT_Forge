#pragma once
#include <Content/Animation/SkinDefinition.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <span>

namespace KT::Content
{
	// Mesh JSON内のSkinだけを厳密に変換する。GPU/再生状態は保存しない。
	[[nodiscard]] nlohmann::json EncodeSkin(const SkinDefinition& definition, std::span<const SkinInfluence> influences);
	[[nodiscard]] std::shared_ptr<const SkinDefinition> DecodeSkin(const nlohmann::json& value, std::vector<SkinInfluence>& influences);
}
