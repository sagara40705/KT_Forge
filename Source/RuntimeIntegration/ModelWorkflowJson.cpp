#include <RuntimeIntegration/ModelWorkflowJson.h>
#include <Content/AssetJson.h>
#include <nlohmann/json.hpp>
#include <fstream>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace KT::RuntimeIntegration
{
	namespace
	{
		using Json = nlohmann::json;

		Json Read(const std::filesystem::path& path)
		{
			std::ifstream input(path, std::ios::binary | std::ios::ate);
			const std::streamoff size = input.tellg();
			if (!input || size <= 0 || size > static_cast<std::streamoff>(KT::Content::AssetJson::MaxBytes))
			{
				throw std::invalid_argument("Model制作設定JSONを開けないか、サイズが64MiBを超えています。");
			}
			std::string text(static_cast<std::size_t>(size), '\0');
			input.seekg(0);
			if (!input.read(text.data(), static_cast<std::streamsize>(size)) || input.peek() != std::char_traits<char>::eof())
			{
				throw std::runtime_error("Model制作設定JSONの読込中にサイズが変わったか、最後まで読み込めません。");
			}
			std::vector<std::set<std::string>> keys;
			return Json::parse(text,
				[&](int depth, Json::parse_event_t event, Json& value)
				{
					if (depth > 6)
					{
						throw std::invalid_argument("Model制作設定JSONの深さが上限6を超えています。");
					}
					if (event == Json::parse_event_t::object_start)
					{
						keys.emplace_back();
					}
					else if (event == Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)
					{
						throw std::invalid_argument("Model制作設定JSONのキーが重複しています。");
					}
					else if (event == Json::parse_event_t::object_end)
					{
						keys.pop_back();
					}
					return true;
				});
		}

		// この保存で新規作成した一時ファイルだけを失敗時に破棄する。
		struct TemporaryFile
		{
			std::filesystem::path path;
			HANDLE handle = INVALID_HANDLE_VALUE;
			bool created = false;

			~TemporaryFile()
			{
				if (handle != INVALID_HANDLE_VALUE)
				{
					CloseHandle(handle);
				}
				if (created)
				{
					DeleteFileW(path.c_str());
				}
			}
		};
	}

	ModelReimportOptions ModelWorkflowJson::ReadCorrespondence(const std::filesystem::path& path)
	{
		try
		{
			const auto json = Read(path);
			if (!json.is_object() || (json.size() != 3 && json.size() != 4) || !json.at("version").is_number_integer() ||
				json.at("version") != 1 || !json.at("nodeKeys").is_object() || (json.size() == 4 && !json.contains("available")))
			{
				throw std::invalid_argument("Modelノード対応JSONにはversion 1、nodeKeys、acceptUnmatchedNodesが必要です。");
			}
			ModelReimportOptions options;
			options.acceptUnmatchedNodes = json.at("acceptUnmatchedNodes").get<bool>();
			std::set<std::string> destinations;
			for (const auto& [oldKey, value] : json.at("nodeKeys").items())
			{
				const auto newKey = value.get<std::string>();
				KT::World::ValidateSceneText(oldKey);
				KT::World::ValidateSceneText(newKey);
				if (oldKey.empty() || newKey.empty() || !destinations.insert(newKey).second)
				{
					throw std::invalid_argument("Modelノード対応に空keyまたは同じ新keyへの重複があります。");
				}
				options.nodeKeys.emplace(oldKey, newKey);
			}
			// availableは編集補助情報。実際の照合先は再取込時に最新入力から検査する。
			if (json.contains("available"))
			{
				const auto& available = json.at("available");
				if (!available.is_object() || available.size() != 2 || !available.at("previous").is_object() ||
					!available.at("current").is_array())
				{
					throw std::invalid_argument("Modelノード対応のavailable形式が不正です。");
				}
				for (const auto& [key, id] : available.at("previous").items())
				{
					KT::World::ValidateSceneText(key);
					(void)KT::Content::ModelNodeId::Parse(id.get<std::string>());
				}
				for (const auto& key : available.at("current"))
				{
					KT::World::ValidateSceneText(key.get<std::string>());
				}
			}
			return options;
		}
		catch (const Json::exception& error)
		{
			throw std::invalid_argument("Modelノード対応JSONの型・フィールド・構文が不正です。JSONエラーID=" + std::to_string(error.id));
		}
	}

	ModelReconcileSelection ModelWorkflowJson::ReadSelection(const std::filesystem::path& path)
	{
		try
		{
			const auto json = Read(path);
			if (!json.is_object() || json.size() != 3 || !json.at("version").is_number_integer() || json.at("version") != 1 ||
				!json.at("roots").is_array() || !json.at("models").is_array())
			{
				throw std::invalid_argument("Model対象JSONにはversion 1、roots配列、models配列が必要です。");
			}
			ModelReconcileSelection selection;
			for (const auto& value : json.at("roots"))
			{
				if (!selection.roots.insert(KT::World::ObjectUuid::Parse(value.get<std::string>())).second)
				{
					throw std::invalid_argument("Model反映対象のrootが重複しています。");
				}
			}
			for (const auto& value : json.at("models"))
			{
				if (!selection.models.insert(KT::Content::ModelAssetId::Parse(value.get<std::string>())).second)
				{
					throw std::invalid_argument("Model反映対象のModel IDが重複しています。");
				}
			}
			return selection;
		}
		catch (const Json::exception& error)
		{
			throw std::invalid_argument("Model対象JSONの型・フィールド・構文が不正です。JSONエラーID=" + std::to_string(error.id));
		}
	}

	void ModelWorkflowJson::SaveNodeTable(const std::filesystem::path& path, const ModelNodeTable& table)
	{
		Json previous = Json::object();
		for (const auto& [key, id] : table.previous)
		{
			previous[key] = id.ToString();
		}
		const auto text = Json{{"version", 1}, {"nodeKeys", Json::object()}, {"acceptUnmatchedNodes", false},
			{"available", {{"previous", std::move(previous)}, {"current", table.current}}}}
							  .dump(2);
		if (text.size() > KT::Content::AssetJson::MaxBytes)
		{
			throw std::invalid_argument("Model対応表JSONが64MiBを超えています。");
		}
		TemporaryFile temporary;
		temporary.path = path;
		temporary.path += "." + KT::Content::AssetId::Generate().ToString() + ".tmp";
		temporary.handle = CreateFileW(temporary.path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (temporary.handle == INVALID_HANDLE_VALUE)
		{
			throw std::runtime_error("Model対応表の一時ファイルを作成できません。Win32=" + std::to_string(GetLastError()));
		}
		temporary.created = true;
		DWORD written = 0;
		if (!WriteFile(temporary.handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) || written != text.size() ||
			!FlushFileBuffers(temporary.handle))
		{
			throw std::runtime_error("Model対応表の書込またはflushに失敗しました。");
		}
		const auto closed = CloseHandle(temporary.handle);
		temporary.handle = INVALID_HANDLE_VALUE;
		if (!closed || !MoveFileExW(temporary.path.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			throw std::runtime_error("Model対応表の保存先置換に失敗しました。Win32=" + std::to_string(GetLastError()));
		}
		temporary.created = false;
	}

	std::vector<KT::Content::AssetId> CollectModelSceneAssets(
		const KT::World::SceneAsset& scene, const KT::World::ComponentRegistry& components)
	{
		std::set<KT::Content::AssetId> ids;
		for (const auto& object : scene.objects)
		{
			for (const auto& component : object.components)
			{
				if (component.type == "kt.ModelInstance")
				{
					ids.insert(KT::World::ModelInstance::Decode(component.data).model.Value());
				}
				if (component.type != "kt.MeshRenderer")
				{
					continue;
				}
				const auto migrated = components.ReadLegacyData(component);
				const auto& renderer = migrated ? *migrated : component.data;
				for (const auto* field : {"mesh", "material"})
				{
					const auto& value = renderer.At(field);
					if (!std::holds_alternative<std::nullptr_t>(value.value))
					{
						ids.insert(KT::Content::AssetId::Parse(value.AsString()));
					}
				}
				for (const auto& [slot, value] : renderer.At("materials").AsObject())
				{
					if (!std::holds_alternative<std::nullptr_t>(value.value))
					{
						ids.insert(KT::Content::AssetId::Parse(value.AsString()));
					}
				}
			}
		}
		return {ids.begin(), ids.end()};
	}
}
