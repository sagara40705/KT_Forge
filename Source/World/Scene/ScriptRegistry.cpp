#include <World/Scene/ScriptRegistry.h>
#include <cmath>
#include <stdexcept>

namespace KT::World
{
	void ScriptRegistry::RegisterEntry(std::string classId, Entry entry)
	{
		if (frozen_)
		{
			throw std::logic_error("固定後のScriptRegistryへ登録できません。");
		}
		if (classId.empty() || entry.version == 0 || !entry.validate || !entry.factory)
		{
			throw std::invalid_argument("Script登録のクラスID・version・検査処理・factoryが不正です。");
		}
		ValidateSceneText(classId);
		for (const auto& [existingId, existing] : entries_)
		{
			if (existingId == classId || existing.cppType == entry.cppType)
			{
				throw std::invalid_argument("ScriptのクラスIDまたはC++型が二重登録されています: " + classId);
			}
		}
		entries_.emplace(std::move(classId), std::move(entry));
	}

	void ScriptRegistry::Freeze() noexcept
	{
		frozen_ = true;
	}

	bool ScriptRegistry::IsFrozen() const noexcept
	{
		return frozen_;
	}

	void ScriptRegistry::Validate(const ScriptDefinition& definition, const std::set<ObjectUuid>& objects) const
	{
		if (!frozen_)
		{
			throw std::logic_error("ScriptRegistryは読込・保存前にFreezeしてください。");
		}
		if (definition.className.empty() || definition.version == 0)
		{
			throw std::invalid_argument("Script設定のクラスIDまたはversionが不正です。");
		}
		ValidateSceneText(definition.className);

		// Missing Scriptも、設定値の有限性とScene内のUUID参照を検査する。
		for (const auto& [name, setting] : definition.settings)
		{
			ValidateSceneText(name);
			if (const auto* text = std::get_if<std::string>(&setting))
			{
				ValidateSceneText(*text);
			}
			if (setting.valueless_by_exception())
			{
				throw std::invalid_argument("Script設定値の型が失効しています: " + name);
			}
			if (const auto* number = std::get_if<double>(&setting); number && !std::isfinite(*number))
			{
				throw std::invalid_argument("Script設定のfloat64は有限値である必要があります: " + name);
			}
			if (const auto* uuid = std::get_if<ObjectUuid>(&setting); uuid && uuid->IsValid() && !objects.contains(*uuid))
			{
				throw std::invalid_argument("Script設定のUUID参照先がScene内にありません: " + name);
			}
		}
		const auto entry = entries_.find(definition.className);
		if (entry == entries_.end())
		{
			return;
		}
		if (entry->second.version != definition.version)
		{
			throw std::invalid_argument("Script設定versionが未対応です: " + definition.className);
		}
		entry->second.validate(definition.settings);
	}

	std::unique_ptr<ScriptBehaviour> ScriptRegistry::Create(const ScriptDefinition& definition, const std::set<ObjectUuid>& objects) const
	{
		Validate(definition, objects);
		const auto entry = entries_.find(definition.className);
		if (entry == entries_.end())
		{
			return nullptr;
		}
		auto instance = entry->second.factory(definition);
		if (!instance)
		{
			throw std::runtime_error("登録済みScriptのfactoryが実体を生成しませんでした: " + definition.className);
		}
		return instance;
	}

	SceneValue ScriptRegistry::Encode(const std::vector<ScriptDefinition>& definitions)
	{
		SceneValue::Array scripts;
		for (const auto& definition : definitions)
		{
			SceneValue::Object settings;
			for (const auto& [name, setting] : definition.settings)
			{
				SceneValue::Object encoded;
				std::visit(
					[&](const auto& input)
					{
						using T = std::decay_t<decltype(input)>;
						if constexpr (std::is_same_v<T, bool>)
						{
							encoded = {{"type", "bool"}, {"value", input}};
						}
						else if constexpr (std::is_same_v<T, std::int64_t>)
						{
							encoded = {{"type", "int64"}, {"value", std::to_string(input)}};
						}
						else if constexpr (std::is_same_v<T, double>)
						{
							encoded = {{"type", "float64"}, {"value", input}};
						}
						else if constexpr (std::is_same_v<T, std::string>)
						{
							encoded = {{"type", "string"}, {"value", input}};
						}
						else
						{
							encoded = {{"type", "objectUuid"}, {"value", input.IsValid() ? SceneValue(input.ToString()) : SceneValue{}}};
						}
					},
					setting);
				settings.emplace(name, std::move(encoded));
			}
			scripts.emplace_back(SceneValue::Object{{"class", definition.className}, {"version", double(definition.version)},
				{"enabled", definition.enabled}, {"settings", std::move(settings)}});
		}
		return SceneValue::Object{{"scripts", std::move(scripts)}};
	}

	std::vector<ScriptDefinition> ScriptRegistry::Decode(const SceneValue& data)
	{
		data.RequireFields({"scripts"});
		std::vector<ScriptDefinition> definitions;
		for (const auto& script : data.At("scripts").AsArray())
		{
			script.RequireFields({"class", "version", "enabled", "settings"});
			ScriptDefinition definition;
			definition.className = script.At("class").AsString();
			definition.version = script.At("version").AsVersion();
			definition.enabled = script.At("enabled").AsBool();
			for (const auto& [name, setting] : script.At("settings").AsObject())
			{
				setting.RequireFields({"type", "value"});
				const auto& type = setting.At("type").AsString();
				const auto& value = setting.At("value");
				if (type == "bool")
				{
					definition.settings.emplace(name, value.AsBool());
				}
				else if (type == "int64")
				{
					definition.settings.emplace(name, value.AsInt64());
				}
				else if (type == "float64")
				{
					definition.settings.emplace(name, value.AsNumber());
				}
				else if (type == "string")
				{
					definition.settings.emplace(name, value.AsString());
				}
				else if (type == "objectUuid")
				{
					definition.settings.emplace(
						name, std::holds_alternative<std::nullptr_t>(value.value) ? ObjectUuid{} : ObjectUuid::Parse(value.AsString()));
				}
				else
				{
					throw std::invalid_argument("Script設定値の型が未対応です: " + type);
				}
			}
			definitions.push_back(std::move(definition));
		}
		return definitions;
	}
}
