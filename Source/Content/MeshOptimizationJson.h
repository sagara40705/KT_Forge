#pragma once
#include <Content/MeshOptimization.h>
#include <nlohmann/json.hpp>

namespace KT::Content
{
	// OBJとAssimpのrecipeで共用する厳密JSON変換。公開最適化APIへJSON型を持ち込まない。
	class MeshOptimizationJson
	{
	public:
		[[nodiscard]] static MeshOptimizationRecord Decode(const nlohmann::json& value);
		[[nodiscard]] static nlohmann::json Encode(const MeshOptimizationRecord& record);
	};
}
