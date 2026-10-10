#include <World/Systems/HierarchySystem.h>
#include <algorithm>
#include <stdexcept>

namespace KT::World
{
	HierarchySnapshot HierarchySystem::Build(std::span<const HierarchyInput> inputs)
	{
		// Entityのスロットindexと、今回のノードindexを対応付ける。
		HierarchySnapshot snapshot;
		snapshot.nodes.reserve(inputs.size());
		snapshot.parentFirst.reserve(inputs.size());
		std::size_t slotCount = 0;
		for (const auto& input : inputs)
		{
			if (!input.entity.IsValid())
			{
				throw std::invalid_argument("階層のEntityが無効です。");
			}
			slotCount = (std::max)(slotCount, static_cast<std::size_t>(input.entity.index) + 1);
		}
		snapshot.nodeByEntityIndex.resize(slotCount, NoParent);
		for (const auto& input : inputs)
		{
			auto& nodeIndex = snapshot.nodeByEntityIndex[input.entity.index];
			if (nodeIndex != NoParent)
			{
				throw std::invalid_argument("階層のEntity slotが重複しています。");
			}
			nodeIndex = snapshot.nodes.size();
			snapshot.nodes.push_back({input.entity, NoParent, {}});
		}

		// 親の生存と所属を検査し、親子のindexを登録する。
		for (std::size_t nodeIndex = 0; nodeIndex < inputs.size(); ++nodeIndex)
		{
			const auto parent = inputs[nodeIndex].parent;
			if (parent == Entity{})
			{
				snapshot.parentFirst.push_back(nodeIndex);
				continue;
			}

			const auto parentIndex = snapshot.FindNode(parent);

			if (parent == inputs[nodeIndex].entity)
			{
				throw std::invalid_argument("Entity自身を階層の親に指定できません。");
			}

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
			throw std::invalid_argument("階層の親子関係が循環しています。");
		}

		// 再帰を使わず、部分木の半開区間を構造変更時だけ作る。
		snapshot.subtreeOrder.reserve(inputs.size());
		snapshot.subtreeBegin.resize(inputs.size());
		snapshot.subtreeEnd.resize(inputs.size());
		snapshot.parentFirstPosition.resize(inputs.size());
		std::vector<std::pair<std::size_t, std::size_t>> stack;
		stack.reserve(inputs.size());
		for (std::size_t position = 0; position < snapshot.parentFirst.size(); ++position)
		{
			const auto root = snapshot.parentFirst[position];
			snapshot.parentFirstPosition[root] = position;
			if (snapshot.nodes[root].parent != NoParent)
			{
				continue;
			}
			snapshot.subtreeBegin[root] = snapshot.subtreeOrder.size();
			snapshot.subtreeOrder.push_back(root);
			stack.emplace_back(root, 0);
			while (!stack.empty())
			{
				auto& [node, nextChild] = stack.back();
				if (nextChild == snapshot.nodes[node].children.size())
				{
					snapshot.subtreeEnd[node] = snapshot.subtreeOrder.size();
					stack.pop_back();
					continue;
				}
				const auto child = snapshot.nodes[node].children[nextChild++];
				snapshot.subtreeBegin[child] = snapshot.subtreeOrder.size();
				snapshot.subtreeOrder.push_back(child);
				stack.emplace_back(child, 0);
			}
		}
		return snapshot;
	}

	void HierarchySystem::Update(SceneUpdateContext& context) const
	{
		// 前段階の完了を確認し、同じ更新の結果だけを使う。
		context.RequireStage(SceneUpdateContext::Stage::Captured);

		try
		{
			// 入力と同時にWorldから捕捉したconst階層を再利用し、再構築しない。
			if (!context.hierarchy_)
			{
				throw std::logic_error("CPU入力に検証済みの階層がありません。");
			}
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
