#include <World/Scene/ComponentRegistry.h>

namespace KT::World
{
	void SceneReadContext::RequireReference(ObjectUuid uuid) const
	{
		if (uuid.IsValid() && !objects.contains(uuid))
		{
			throw std::invalid_argument("Component設定のUUID参照先がScene内にありません。");
		}
		if (uuid.IsValid() && references)
		{
			references->push_back(uuid);
		}
	}

	void ComponentRegistry::Register(ComponentDescriptor descriptor)
	{
		if (frozen_)
		{
			throw std::logic_error("固定後のComponentRegistryへ登録できません。");
		}
		if (descriptor.type.empty() || descriptor.version == 0 || descriptor.cppType == typeid(void) || !descriptor.capture ||
			!descriptor.validate || !descriptor.restore)
		{
			throw std::invalid_argument("Component登録の型ID・C++型・version・callbackが不正です。");
		}
		ValidateSceneText(descriptor.type);
		if (descriptor.cppType == typeid(PersistentId) || descriptor.cppType == typeid(Name) || descriptor.cppType == typeid(Hierarchy) ||
			descriptor.type == "kt.PersistentId" || descriptor.type == "kt.Name" || descriptor.type == "kt.Hierarchy")
		{
			throw std::invalid_argument("UUID・名前・親はScene共通情報のため、ComponentRegistryへ登録できません。");
		}
		for (const auto& [type, existing] : entries_)
		{
			if (type == descriptor.type || existing.cppType == descriptor.cppType)
			{
				throw std::invalid_argument("Componentの型IDまたはC++型が二重登録されています: " + descriptor.type);
			}
		}
		const auto type = descriptor.type;
		entries_.emplace(type, std::move(descriptor));
	}

	void ComponentRegistry::Freeze() noexcept
	{
		frozen_ = true;
	}

	bool ComponentRegistry::IsFrozen() const noexcept
	{
		return frozen_;
	}

	const ComponentDescriptor& ComponentRegistry::Find(const std::string& type) const
	{
		if (!frozen_)
		{
			throw std::logic_error("ComponentRegistryは読込・保存前にFreezeしてください。");
		}
		const auto entry = entries_.find(type);
		if (entry == entries_.end())
		{
			throw std::invalid_argument("SceneのComponent型が未登録です: " + type);
		}
		return entry->second;
	}

	const ComponentDescriptor& ComponentRegistry::Find(std::type_index cppType) const
	{
		if (!frozen_)
		{
			throw std::logic_error("ComponentRegistryは読込・保存前にFreezeしてください。");
		}
		for (const auto& [type, entry] : entries_)
		{
			if (entry.cppType == cppType)
			{
				return entry;
			}
		}
		throw std::invalid_argument("Sceneに保存未対応のComponentがあります: " + std::string(cppType.name()));
	}

	void ComponentRegistry::Validate(const SceneComponentDefinition& definition, const SceneReadContext& context) const
	{
		const auto& entry = Find(definition.type);
		const auto data = ReadLegacyData(definition);
		(void)definition.data.AsObject();
		definition.data.Validate();
		try
		{
			entry.validate(data ? *data : definition.data, context);
			// 直接登録した参照列挙もScene内の存在を検査し、制作時だけの契約にしない。
			if (entry.enumerateReferences)
			{
				entry.enumerateReferences(data ? *data : definition.data, context);
			}
		}
		catch (const std::invalid_argument& error)
		{
			throw std::invalid_argument("Component設定の検査に失敗しました: " + definition.type + " / " + error.what());
		}
	}

	std::optional<std::vector<ObjectUuid>> ComponentRegistry::References(
		const SceneComponentDefinition& definition, const SceneReadContext& context) const
	{
		const auto& descriptor = Find(definition.type);
		if (!descriptor.enumerateReferences)
		{
			return std::nullopt;
		}
		const auto migrated = ReadLegacyData(definition);
		std::vector<ObjectUuid> references;
		const SceneReadContext enumeration{context.objects, context.scripts, &references};
		descriptor.enumerateReferences(migrated ? *migrated : definition.data, enumeration);
		return references;
	}

	std::optional<SceneValue> ComponentRegistry::ReadLegacyData(const SceneComponentDefinition& definition) const
	{
		const auto& entry = Find(definition.type);
		if (entry.version == definition.version)
		{
			return std::nullopt;
		}
		if (entry.migrate)
		{
			return entry.migrate(definition.version, definition.data);
		}
		throw std::invalid_argument("Component設定versionが未対応です: " + definition.type);
	}
}
