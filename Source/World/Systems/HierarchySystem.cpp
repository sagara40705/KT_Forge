#include <World/Systems/HierarchySystem.h>
#include <map>
#include <stdexcept>
#include <utility>

namespace KT::World
{
	HierarchySnapshot HierarchySystem::Build(std::span<const SceneEntityInput> inputs, std::optional<std::pair<Entity, Entity>> replacement)
	{
		HierarchySnapshot result;
		std::map<std::uint32_t, std::size_t> byIndex;
		result.nodes.reserve(inputs.size());
		result.parentFirst.reserve(inputs.size());
		for (const auto& input : inputs)
		{
			byIndex.emplace(input.entity.index, result.nodes.size());
			result.nodes.push_back({input.entity, NoParent, {}});
		}
		for (std::size_t i = 0; i < inputs.size(); ++i)
		{
			auto parent = inputs[i].hierarchy.parent;
			if (replacement && replacement->first == inputs[i].entity)
			{
				parent = replacement->second;
			}
			if (parent == Entity{})
			{
				result.parentFirst.push_back(i);
				continue;
			}
			const auto found = byIndex.find(parent.index);
			if (found == byIndex.end() || inputs[found->second].entity != parent)
			{
				throw std::invalid_argument("Hierarchy parent is stale, invalid or from another World.");
			}
			if (parent == inputs[i].entity)
			{
				throw std::invalid_argument("Entity cannot be its own parent.");
			}
			const auto p = found->second;
			result.nodes[i].parent = p;
			result.nodes[p].children.push_back(i);
		}
		for (std::size_t i = 0; i < result.parentFirst.size(); ++i)
		{
			for (auto child : result.nodes[result.parentFirst[i]].children)
			{
				result.parentFirst.push_back(child);
			}
		}
		if (result.parentFirst.size() != inputs.size())
		{
			throw std::invalid_argument("Hierarchy contains a cycle.");
		}
		return result;
	}

	void HierarchySystem::Update(SceneUpdateContext& context) const
	{
		context.RequireStage(SceneUpdateContext::Stage::Captured);
		try
		{
			auto result = Build(context.inputs_);
			context.hierarchy_ = std::move(result);
			context.stage_ = SceneUpdateContext::Stage::Hierarchy;
		}
		catch (...)
		{
			context.Fail();
			throw;
		}
	}
}
