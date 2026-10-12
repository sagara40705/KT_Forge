#include <World/Scene/SceneJson.h>
#include <World/Physics/PhysicsSerialization.h>
#include <nlohmann/json.hpp>
#include <Platform/FileReplacement.h>
#include <algorithm>
#include <cmath>
#include <fstream>

namespace KT::World
{
	using Json = nlohmann::json;

	// JSONの数値をCPU設定へ変換する。64bit整数を数値から丸めて復元しない。
	static SceneValue FromJson(const Json& input)
	{
		if (input.is_null())
		{
			return {};
		}
		if (input.is_boolean())
		{
			return input.get<bool>();
		}
		if (input.is_string())
		{
			return input.get<std::string>();
		}
		if (input.is_number())
		{
			constexpr std::uint64_t exactIntegerLimit = 9007199254740991ULL;
			if (input.is_number_unsigned() && input.get<std::uint64_t>() > exactIntegerLimit)
			{
				throw std::invalid_argument("JSONの大きな整数は10進文字列で保存してください。");
			}
			if (input.is_number_integer() && !input.is_number_unsigned())
			{
				const auto number = input.get<std::int64_t>();
				if (number < -static_cast<std::int64_t>(exactIntegerLimit) || number > static_cast<std::int64_t>(exactIntegerLimit))
				{
					throw std::invalid_argument("JSONの大きな整数は10進文字列で保存してください。");
				}
			}
			return input.get<double>();
		}
		if (input.is_array())
		{
			SceneValue::Array result;
			for (const auto& element : input)
			{
				result.push_back(FromJson(element));
			}
			return result;
		}
		SceneValue::Object result;
		for (const auto& [key, element] : input.items())
		{
			result.emplace(key, FromJson(element));
		}
		return result;
	}

	static Json ToJson(const SceneValue& input, std::size_t depth = 0)
	{
		if (depth > SceneJson::MaxDepth)
		{
			throw std::invalid_argument("JSON設定の階層が128を超えています。");
		}
		return std::visit(
			[depth](const auto& value) -> Json
			{
				using T = std::decay_t<decltype(value)>;
				if constexpr (std::is_same_v<T, SceneValue::Array>)
				{
					auto result = Json::array();
					for (const auto& element : value)
					{
						result.push_back(ToJson(element, depth + 1));
					}
					return result;
				}
				else if constexpr (std::is_same_v<T, SceneValue::Object>)
				{
					auto result = Json::object();
					for (const auto& [key, element] : value)
					{
						result[key] = ToJson(element, depth + 1);
					}
					return result;
				}
				else
				{
					return Json(value);
				}
			},
			input.value);
	}

	SceneAsset SceneJson::Parse(std::string_view text, const ComponentRegistry& components, const ScriptRegistry& scripts,
		const LegacyMeshRendererAssets* legacyAssets)
	{
		if (text.size() > MaxBytes)
		{
			throw std::invalid_argument("Scene JSONが初版の64MiB上限を超えています。");
		}
		try
		{
			// objectごとのキー集合を保持し、DOMで上書きされる前に重複を拒否する。
			std::vector<std::set<std::string>> keys;
			const auto document = Json::parse(text.begin(), text.end(),
				[&](int depth, Json::parse_event_t event, Json& parsed)
				{
					if (depth > MaxDepth)
					{
						throw std::invalid_argument("Scene JSONの階層が128を超えています。");
					}
					if (event == Json::parse_event_t::object_start)
					{
						keys.emplace_back();
					}
					else if (event == Json::parse_event_t::key)
					{
						if (!keys.back().insert(parsed.get<std::string>()).second)
						{
							throw std::invalid_argument("Scene JSONのキーが重複しています: " + parsed.get<std::string>());
						}
					}
					else if (event == Json::parse_event_t::object_end)
					{
						keys.pop_back();
					}
					return true;
				});
			const auto root = FromJson(document);
			root.Validate();
			if (root.At("version").AsVersion() == 2)
			{
				root.RequireFields({"format", "version", "name", "objects", "physics"});
			}
			else
			{
				root.RequireFields({"format", "version", "name", "objects"});
			}
			if (root.At("format").AsString() != "KT_Forge.Scene")
			{
				throw std::invalid_argument("JSONのformatがKT_Forge.Sceneではありません。");
			}

			SceneAsset asset;
			asset.version = root.At("version").AsVersion();
			asset.name = root.At("name").AsString();
			if (asset.version == 2)
			{
				asset.physics = DecodePhysicsSettings(root.At("physics"));
			}
			for (const auto& input : root.At("objects").AsArray())
			{
				input.RequireFields({"uuid", "name", "parent", "components"});
				SceneObjectDefinition object;
				object.uuid = ObjectUuid::Parse(input.At("uuid").AsString());
				object.name = input.At("name").AsString();
				if (!std::holds_alternative<std::nullptr_t>(input.At("parent").value))
				{
					object.parent = ObjectUuid::Parse(input.At("parent").AsString());
				}
				for (const auto& component : input.At("components").AsArray())
				{
					component.RequireFields({"type", "version", "data"});
					object.components.push_back(
						{component.At("type").AsString(), component.At("version").AsVersion(), component.At("data")});
				}
				asset.objects.push_back(std::move(object));
			}
			if (legacyAssets)
			{
				asset = MigrateMeshRendererAssets(asset, *legacyAssets);
			}
			SceneLoader::Validate(asset, components, scripts);
			return asset;
		}
		catch (const Json::parse_error& error)
		{
			throw std::invalid_argument("Scene JSONの構文またはUTF-8が不正です。byte位置=" + std::to_string(error.byte));
		}
		catch (const Json::exception& error)
		{
			throw std::invalid_argument("Scene JSONの数値またはデータ型が不正です。JSONエラーID=" + std::to_string(error.id));
		}
	}

	std::string SceneJson::Stringify(const SceneAsset& asset, const ComponentRegistry& components, const ScriptRegistry& scripts)
	{
		SceneLoader::Validate(asset, components, scripts);
		try
		{
			// 入力の順序を変更せず、UUID・型ID順の参照一覧から出力する。
			std::vector<const SceneObjectDefinition*> objects;
			for (const auto& object : asset.objects)
			{
				objects.push_back(&object);
			}
			std::sort(objects.begin(), objects.end(), [](const auto* first, const auto* second) { return first->uuid < second->uuid; });
			Json document{{"format", "KT_Forge.Scene"}, {"version", asset.version}, {"name", asset.name}, {"objects", Json::array()}};
			if (asset.physics)
			{
				document["physics"] = ToJson(EncodePhysics(*asset.physics), 1);
			}
			for (const auto* object : objects)
			{
				std::vector<const SceneComponentDefinition*> definitions;
				for (const auto& component : object->components)
				{
					definitions.push_back(&component);
				}
				std::sort(definitions.begin(), definitions.end(),
					[](const auto* first, const auto* second) { return first->type < second->type; });
				Json output{{"uuid", object->uuid.ToString()}, {"name", object->name},
					{"parent", object->parent ? Json(object->parent->ToString()) : Json(nullptr)}, {"components", Json::array()}};
				for (const auto* component : definitions)
				{
					output["components"].push_back(
						Json{{"type", component->type}, {"version", component->version}, {"data", ToJson(component->data, 5)}});
				}
				document["objects"].push_back(std::move(output));
			}
			auto text = document.dump(2) + "\n";
			if (text.size() > MaxBytes)
			{
				throw std::invalid_argument("Scene JSONの出力が初版の64MiB上限を超えています。");
			}
			return text;
		}
		catch (const Json::exception& error)
		{
			throw std::invalid_argument("Scene JSONの書き出しに失敗しました。JSONエラーID=" + std::to_string(error.id));
		}
	}

	SceneAsset SceneJson::Read(const std::filesystem::path& path, const ComponentRegistry& components, const ScriptRegistry& scripts,
		const LegacyMeshRendererAssets* legacyAssets)
	{
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file)
		{
			throw std::runtime_error("Scene JSONファイルを開けません。");
		}
		const auto size = file.tellg();
		if (size < 0 || size > static_cast<std::streamoff>(MaxBytes))
		{
			throw std::runtime_error("Scene JSONファイルのサイズが不正、または64MiBを超えています。");
		}
		std::string text(static_cast<std::size_t>(size), '\0');
		file.seekg(0);
		file.read(text.data(), static_cast<std::streamsize>(text.size()));
		if (!file || file.peek() != std::char_traits<char>::eof())
		{
			throw std::runtime_error("Scene JSONファイルの読込に失敗したか、読込中にサイズが変わりました。");
		}
		return Parse(text, components, scripts, legacyAssets);
	}
	void SceneJson::Save(
		const std::filesystem::path& path, const Scene& scene, const ComponentRegistry& components, const ScriptRegistry& scripts)
	{
		Save(path, SceneLoader::Capture(scene, components, scripts), components, scripts);
	}

	void SceneJson::Save(
		const std::filesystem::path& path, const SceneAsset& scene, const ComponentRegistry& components, const ScriptRegistry& scripts)
	{
		// 保存可能な完成状態と設定を検査し、既存ファイルへ触れる前に全JSONを作る。
		const auto text = Stringify(scene, components, scripts);
		KT::Platform::ReplaceFileWithText(path, text, ObjectUuid::Generate().ToString());
	}
}
