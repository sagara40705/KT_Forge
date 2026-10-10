#include <World/Systems/HierarchySystem.h>
#include <map>
#include <stdexcept>
#include <utility>

namespace KT::World
{
	HierarchySnapshot HierarchySystem::Build(std::span<const SceneEntityInput> inputs, std::optional<std::pair<Entity, Entity>> replacement)
	{
		// Entityのスロットindexと、今回のノードindexを対応付ける。
		HierarchySnapshot snapshot;
		std::map<std::uint32_t, std::size_t> nodeByEntityIndex;
		snapshot.nodes.reserve(inputs.size());
		snapshot.parentFirst.reserve(inputs.size());
		for (const auto& input : inputs)
		{
			nodeByEntityIndex.emplace(input.entity.index, snapshot.nodes.size());
			snapshot.nodes.push_back({input.entity, NoParent, {}});
		}

		// 親の生存と所属を検査し、親子のindexを登録する。
		for (std::size_t nodeIndex = 0; nodeIndex < inputs.size(); ++nodeIndex)
		{
			auto parent = inputs[nodeIndex].hierarchy.parent;
			if (replacement && replacement->first == inputs[nodeIndex].entity)
			{
				parent = replacement->second;
			}
			if (parent == Entity{})
			{
				snapshot.parentFirst.push_back(nodeIndex);
				continue;
			}

			const auto parentIterator = nodeByEntityIndex.find(parent.index);
			if (parentIterator == nodeByEntityIndex.end() || inputs[parentIterator->second].entity != parent)
			{
				throw std::invalid_argument("Hierarchy parent is stale, invalid or from another World.");
			}

			if (parent == inputs[nodeIndex].entity)
			{
				throw std::invalid_argument("Entity cannot be its own parent.");
			}

			const auto parentIndex = parentIterator->second;
			snapshot.nodes[nodeIndex].parent = parentIndex;
			snapshot.nodes[parentIndex].children.push_back(nodeIndex);
		}

		// 根から子へ順序を広げ、未到達のノードがあれば循環として拒否する。
		for (std::size_t nodeIndex = 0; nodeIndex < snapshot.parentFirst.size(); ++nodeIndex)
		{
			for (auto child : snapshot.nodes[snapshot.parentFirst[nodeIndex]].children)
			{
				snapshot.parentFirst.push_back(child);
			}
		}

		if (snapshot.parentFirst.size() != inputs.size())
		{
			throw std::invalid_argument("Hierarchy contains a cycle.");
		}

		return snapshot;
	}

	void HierarchySystem::Update(SceneUpdateContext& context) const
	{
		// 前段階の完了を確認し、同じ更新の結果だけを使う。
		context.RequireStage(SceneUpdateContext::Stage::Captured);

		try
		{
			auto snapshot = Build(context.inputs_);
			context.hierarchy_ = std::move(snapshot);
			context.stage_ = SceneUpdateContext::Stage::Hierarchy;
		}
		catch (...)
		{
			// 失敗した更新の続行を禁止し、元の例外を返す。
			context.Fail();
			throw;
		}
	}
}
