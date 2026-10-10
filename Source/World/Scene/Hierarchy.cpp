#include <World/Scene/Hierarchy.h>
#include <stdexcept>

namespace KT::World
{
	namespace
	{
		void RequireEntity(const World& world, Entity entity)
		{
			if (!world.IsAlive(entity))
			{
				throw std::invalid_argument("HierarchyのEntityが失効/別Worldです。");
			}
		}

	}

	std::size_t HierarchySnapshot::FindNode(Entity entity) const
	{
		// slotだけで一致させず、捕捉時のWorldと世代も含めて照合する。
		if (entity.index >= nodeByEntityIndex.size())
		{
			throw std::invalid_argument("階層索引のEntity slotが範囲外です。");
		}
		const auto nodeIndex = nodeByEntityIndex[entity.index];
		if (nodeIndex == NoParent || nodeIndex >= nodes.size() || nodes[nodeIndex].entity != entity)
		{
			throw std::invalid_argument("階層索引のEntityが空きslot・失効世代・別Worldです。");
		}
		return nodeIndex;
	}

	HierarchySnapshot ValidateHierarchy(const World& world)
	{
		// 値を返す既存APIは維持し、検証済みの共有階層からコピーする。
		return *world.GetHierarchy();
	}

	void SetParent(World& world, Entity child, Entity parent, ParentChangeMode mode)
	{
		world.SetParent(child, parent, mode);
	}

	Entity GetParent(const World& world, Entity child)
	{
		RequireEntity(world, child);

		const auto snapshot = world.GetHierarchy();
		const auto parentIndex = snapshot->nodes[snapshot->FindNode(child)].parent;
		return parentIndex == NoParent ? Entity{} : snapshot->nodes[parentIndex].entity;
	}

	std::vector<Entity> GetChildren(const World& world, Entity parent)
	{
		RequireEntity(world, parent);

		const auto snapshot = world.GetHierarchy();

		// 検証済みの子indexをEntityへ戻す。
		std::vector<Entity> children;
		const auto& childIndices = snapshot->nodes[snapshot->FindNode(parent)].children;
		children.reserve(childIndices.size());
		for (auto index : childIndices)
		{
			children.push_back(snapshot->nodes[index].entity);
		}

		return children;
	}

	void DestroySubtree(World& world, Entity root)
	{
		// 公開削除はすべて、同じ部分木削除の契約を使う。
		world.DestroyEntity(root);
	}
}
