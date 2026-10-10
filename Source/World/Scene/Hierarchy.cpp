#include <World/Scene/Hierarchy.h>
#include <World/Systems/HierarchySystem.h>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>

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

		std::size_t FindNode(const HierarchySnapshot& snapshot, Entity entity)
		{
			for (std::size_t nodeIndex = 0; nodeIndex < snapshot.nodes.size(); ++nodeIndex)
			{
				if (snapshot.nodes[nodeIndex].entity == entity)
				{
					return nodeIndex;
				}
			}

			throw std::invalid_argument("HierarchyにEntityがありません。");
		}
	}

	HierarchySnapshot ValidateHierarchy(const World& world)
	{
		// 入力を捕捉し、更新処理と同じ規則で階層を検証する。
		SceneUpdateContext context(world, std::nullopt, {});
		return HierarchySystem::Build(context.Inputs());
	}

	void SetParent(World& world, Entity child, Entity parent)
	{
		world.SetParent(child, parent);
	}

	Entity GetParent(const World& world, Entity child)
	{
		RequireEntity(world, child);

		const auto snapshot = ValidateHierarchy(world);
		const auto parentIndex = snapshot.nodes[FindNode(snapshot, child)].parent;
		return parentIndex == NoParent ? Entity{} : snapshot.nodes[parentIndex].entity;
	}

	std::vector<Entity> GetChildren(const World& world, Entity parent)
	{
		RequireEntity(world, parent);

		const auto snapshot = ValidateHierarchy(world);

		// 検証済みの子indexをEntityへ戻す。
		std::vector<Entity> children;
		for (auto index : snapshot.nodes[FindNode(snapshot, parent)].children)
		{
			children.push_back(snapshot.nodes[index].entity);
		}

		return children;
	}

	void DestroySubtree(World& world, Entity root)
	{
		// 公開削除はすべて、同じ部分木削除の契約を使う。
		world.DestroyEntity(root);
	}
}