#include <RuntimeIntegration/ModelReconcileJson.h>
#include <Content/AssetJson.h>
#include <nlohmann/json.hpp>
#include <fstream>

namespace KT::RuntimeIntegration
{
	ModelReconcileOptions ReadModelResolutions(const std::filesystem::path& path)
	{
		std::ifstream input(path, std::ios::binary | std::ios::ate);
		const std::streamoff size = input.tellg();
		if (!input || size <= 0 || size > static_cast<std::streamoff>(KT::Content::AssetJson::MaxBytes))
		{
			throw std::invalid_argument("Model競合解決JSONを開けないか、サイズが不正です。");
		}
		std::string text(static_cast<std::size_t>(size), '\0');
		input.seekg(0);
		if (!input.read(text.data(), static_cast<std::streamsize>(size)))
		{
			throw std::runtime_error("Model競合解決JSONを最後まで読み込めません。");
		}
		try
		{
			std::vector<std::set<std::string>> keys;
			const auto json = nlohmann::json::parse(text,
				[&](int depth, nlohmann::json::parse_event_t event, nlohmann::json& value)
				{
					if (depth > 5)
					{
						throw std::invalid_argument("Model競合解決JSONの深さが上限を超えています。");
					}
					if (event == nlohmann::json::parse_event_t::object_start)
					{
						keys.emplace_back();
					}
					else if (event == nlohmann::json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)
					{
						throw std::invalid_argument("Model競合解決JSONのキーが重複しています。");
					}
					else if (event == nlohmann::json::parse_event_t::object_end)
					{
						keys.pop_back();
					}
					return true;
				});
			if (!json.is_object() || json.size() != 2 || !json.at("version").is_number_integer() || json.at("version") != 1 ||
				!json.at("nodes").is_array())
			{
				throw std::invalid_argument("Model競合解決JSONにはversion 1とnodes配列が必要です。");
			}
			ModelReconcileOptions options;
			for (const auto& entry : json.at("nodes"))
			{
				if (!entry.is_object() || (entry.size() != 5 && entry.size() != 6) || (entry.size() == 6 && !entry.contains("restore")) ||
					!entry.at("followFields").is_array())
				{
					throw std::invalid_argument("Model競合解決ノードの設定フィールドが不正です。");
				}
				const auto key = std::pair{KT::World::ObjectUuid::Parse(entry.at("root").get<std::string>()),
					KT::Content::ModelNodeId::Parse(entry.at("node").get<std::string>())};
				ModelNodeResolution resolution;
				const auto parent = entry.at("parent").get<std::string>();
				if (parent == "KeepLocal")
				{
					resolution.parent = ModelParentResolution::KeepLocal;
				}
				else if (parent == "KeepWorld")
				{
					resolution.parent = ModelParentResolution::KeepWorld;
				}
				else if (parent == "FollowSource")
				{
					resolution.parent = ModelParentResolution::FollowSource;
				}
				else if (parent != "Ask")
				{
					throw std::invalid_argument("Model競合解決の親変更方針が未対応です。");
				}
				resolution.detach = entry.at("detach").get<bool>();
				if (entry.contains("restore"))
				{
					resolution.restore = entry.at("restore").get<bool>();
				}
				for (const auto& field : entry.at("followFields"))
				{
					const auto name = field.get<std::string>();
					KT::World::ModelInstance::ValidateOverride(name);
					if (!resolution.followFields.insert(name).second)
					{
						throw std::invalid_argument("Model競合解決のfollowFieldsが重複しています。");
					}
				}
				if (!options.emplace(key, std::move(resolution)).second)
				{
					throw std::invalid_argument("Model競合解決ノードが重複しています。");
				}
			}
			return options;
		}
		catch (const nlohmann::json::exception& error)
		{
			throw std::invalid_argument("Model競合解決JSONの構文・フィールド・型が不正です。JSONエラーID=" + std::to_string(error.id));
		}
	}
}
