#include <Content/MeshOptimizationJson.h>
#include <nlohmann/json.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace KT::Content
{
	MeshOptimizationRecord MeshOptimizationJson::Decode(const nlohmann::json& value)
	{
		if (!value.is_object() || value.size() != 4 || !value.contains("mode") || !value.contains("overdrawThreshold") ||
			!value.contains("pipelineVersion") || !value.contains("meshoptimizer"))
		{
			throw std::invalid_argument("Mesh最適化JSONの必須フィールドが不足、または未知フィールドがあります。");
		}
		MeshOptimizationRecord record;
		if (value.at("mode") == "none")
		{
			record.settings.mode = MeshOptimizationMode::None;
		}
		else if (value.at("mode") == "standard")
		{
			record.settings.mode = MeshOptimizationMode::Standard;
		}
		else if (value.at("mode") == "overdraw")
		{
			record.settings.mode = MeshOptimizationMode::StandardOverdraw;
		}
		else
		{
			throw std::invalid_argument("Mesh最適化JSONのmodeにはnone/standard/overdrawが必要です。");
		}
		const auto& threshold = value.at("overdrawThreshold");
		if (!threshold.is_number())
		{
			throw std::invalid_argument("Mesh最適化JSONのOverdraw thresholdには数値が必要です。");
		}
		const auto number = threshold.get<double>();
		if (!std::isfinite(number) || number < 1 || number > (std::numeric_limits<float>::max)())
		{
			throw std::invalid_argument("Mesh最適化JSONのOverdraw thresholdが有限floatの1以上の範囲外です。");
		}
		record.settings.overdrawThreshold = static_cast<float>(number);
		const auto& version = value.at("pipelineVersion");
		if (!version.is_number_integer() || version <= 0 || version > (std::numeric_limits<std::uint32_t>::max)())
		{
			throw std::invalid_argument("Mesh最適化JSONのpipelineVersionには正のuint32整数が必要です。");
		}
		record.pipelineVersion = version.get<std::uint32_t>();
		if (!value.at("meshoptimizer").is_string())
		{
			throw std::invalid_argument("Mesh最適化JSONのmeshoptimizerにはコミットSHA文字列が必要です。");
		}
		record.libraryRevision = value.at("meshoptimizer").get<std::string>();
		record.Validate();
		return record;
	}

	nlohmann::json MeshOptimizationJson::Encode(const MeshOptimizationRecord& record)
	{
		record.Validate();
		const auto mode = record.settings.mode == MeshOptimizationMode::None ? "none" :
			record.settings.mode == MeshOptimizationMode::Standard			 ? "standard" :
																			   "overdraw";
		return {{"mode", mode}, {"overdrawThreshold", record.settings.overdrawThreshold}, {"pipelineVersion", record.pipelineVersion},
			{"meshoptimizer", record.libraryRevision}};
	}
}
