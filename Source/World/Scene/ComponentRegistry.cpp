#include <World/Scene/ComponentRegistry.h>

namespace KT::World
{
	void SceneReadContext::RequireReference(ObjectUuid uuid) const
	{
		if (uuid.IsValid() && !objects.contains(uuid))
		{
			throw std::invalid_argument("Component設定のUUID参照先がScene内にありません。");
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
		if (entry.version != definition.version)
		{
			throw std::invalid_argument("Component設定versionが未対応です: " + definition.type);
		}
		(void)definition.data.AsObject();
		definition.data.Validate();
		try
		{
			entry.validate(definition.data, context);
		}
		catch (const std::invalid_argument& error)
		{
			throw std::invalid_argument("Component設定の検査に失敗しました: " + definition.type + " / " + error.what());
		}
	}
}
