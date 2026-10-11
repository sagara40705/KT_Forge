#include <World/Scene/ModelInstance.h>
#include <World/Scene/ComponentRegistry.h>
#include <algorithm>

namespace KT::World
{
	void ModelInstance::ValidateOverride(std::string_view field)
	{
		if (field != "name" && field != "parent" && field != "position" && field != "rotation" && field != "scale" && field != "mesh" &&
			field != "material" && field != "visible" && !(field.starts_with("materials:") && field.size() > 10 && field.size() <= 266))
		{
			throw std::invalid_argument("ModelInstanceのoverride対象が不正です: " + std::string(field));
		}
		ValidateSceneText(field);
	}

	SceneValue ModelInstance::Encode() const
	{
		SceneValue::Array bindings;
		for (const auto& binding : nodes)
		{
			SceneValue::Array overrides;
			for (const auto& field : binding.overrides)
			{
				overrides.emplace_back(field);
			}
			bindings.emplace_back(SceneValue::Object{{"node", binding.node.ToString()}, {"object", binding.object.ToString()},
				{"state",
					binding.state == ModelNodeState::Present		? "Present" :
						binding.state == ModelNodeState::Suppressed ? "Suppressed" :
																	  "Detached"},
				{"source", binding.source}, {"overrides", std::move(overrides)}});
		}
		return SceneValue::Object{{"model", model.ToString()}, {"revision", revision}, {"nodes", std::move(bindings)}};
	}

	ModelInstance ModelInstance::Decode(const SceneValue& value)
	{
		value.RequireFields({"model", "revision", "nodes"});
		ModelInstance instance{KT::Content::ModelAssetId::Parse(value.At("model").AsString()), value.At("revision").AsString(), {}};
		if (instance.revision.size() != 64 ||
			!std::all_of(instance.revision.begin(), instance.revision.end(),
				[](char character) { return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f'); }))
		{
			throw std::invalid_argument("ModelInstanceのRevisionにはSHA-256の小文字hexが必要です。");
		}
		const auto& nodes = value.At("nodes").AsArray();
		if (nodes.empty() || nodes.size() > 100000)
		{
			throw std::invalid_argument("ModelInstanceのノード対応数には1..10万が必要です。");
		}
		std::set<KT::Content::ModelNodeId> ids;
		std::set<ObjectUuid> objects;
		for (const auto& entry : nodes)
		{
			entry.RequireFields({"node", "object", "state", "source", "overrides"});
			ModelNodeBinding binding{KT::Content::ModelNodeId::Parse(entry.At("node").AsString()),
				ObjectUuid::Parse(entry.At("object").AsString()), ModelNodeState::Present, entry.At("source"), {}};
			if (!binding.object.IsValid() || !ids.insert(binding.node).second || !objects.insert(binding.object).second)
			{
				throw std::invalid_argument("ModelInstanceのノードIDまたはObjectUuidが無効・重複です。");
			}
			const auto& state = entry.At("state").AsString();
			if (state == "Suppressed")
			{
				binding.state = ModelNodeState::Suppressed;
			}
			else if (state == "Detached")
			{
				binding.state = ModelNodeState::Detached;
			}
			else if (state != "Present")
			{
				throw std::invalid_argument("ModelInstanceのノード状態が未対応です。");
			}
			binding.source.RequireFields({"name", "parent", "transform", "renderer"});
			(void)binding.source.At("name").AsString();
			if (!std::holds_alternative<std::nullptr_t>(binding.source.At("parent").value))
			{
				(void)KT::Content::ModelNodeId::Parse(binding.source.At("parent").AsString());
			}
			for (const auto& field : entry.At("overrides").AsArray())
			{
				ValidateOverride(field.AsString());
				if (!binding.overrides.insert(field.AsString()).second)
				{
					throw std::invalid_argument("ModelInstanceのoverrideが重複しています。");
				}
			}
			instance.nodes.push_back(std::move(binding));
		}
		// 歴史的な抑止ノードも含め、原本の親照合先が閉じた非循環の対応表であることを要求する。
		std::map<KT::Content::ModelNodeId, const ModelNodeBinding*> bindings;
		for (const auto& binding : instance.nodes)
		{
			bindings.emplace(binding.node, &binding);
		}
		for (const auto& binding : instance.nodes)
		{
			std::set<KT::Content::ModelNodeId> chain;
			const auto* current = &binding;
			while (!std::holds_alternative<std::nullptr_t>(current->source.At("parent").value))
			{
				const auto parent = KT::Content::ModelNodeId::Parse(current->source.At("parent").AsString());
				if (!bindings.contains(parent) || !chain.insert(parent).second || chain.size() > 128)
				{
					throw std::invalid_argument("ModelInstanceの原本親参照が欠落・循環・深さ128超過です。");
				}
				current = bindings.at(parent);
			}
		}
		return instance;
	}
}
