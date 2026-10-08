#include <World/Hierarchy.h>
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
			if (!world.IsAlive(entity)) throw std::invalid_argument("HierarchyのEntityが失効/別Worldです。");
		}
		std::size_t FindNode(const HierarchySnapshot& snapshot, Entity entity)
		{
			for (std::size_t i=0; i<snapshot.nodes.size(); ++i) if (snapshot.nodes[i].entity==entity) return i;
			throw std::invalid_argument("HierarchyにEntityがありません。");
		}
	}
	HierarchySnapshot ValidateHierarchy(const World& world)
	{
		SceneUpdateContext context(world, std::nullopt, {});
		return HierarchySystem::Build(context.Inputs());
	}
	void SetParent(World& world, Entity child, Entity parent)
	{
		world.RequireStructuralChange(); RequireEntity(world,child);
		if (parent!=Entity{}) RequireEntity(world,parent);
		SceneUpdateContext context(world, std::nullopt, {});
		(void)HierarchySystem::Build(context.Inputs(), std::pair{child,parent});
		if (auto* input=world.FindComponent<Hierarchy>(child)) input->parent=parent;
		else world.AddComponent<Hierarchy>(child,parent);
	}
	Entity GetParent(const World& world, Entity child)
	{
		RequireEntity(world,child);
		const auto snapshot=ValidateHierarchy(world);
		const auto p=snapshot.nodes[FindNode(snapshot,child)].parent;
		return p==NoParent ? Entity{} : snapshot.nodes[p].entity;
	}
	std::vector<Entity> GetChildren(const World& world, Entity parent)
	{
		RequireEntity(world,parent); const auto snapshot=ValidateHierarchy(world);
		std::vector<Entity> result;
		for (auto index:snapshot.nodes[FindNode(snapshot,parent)].children) result.push_back(snapshot.nodes[index].entity);
		return result;
	}
	void DestroySubtree(World& world, Entity root)
	{
		world.RequireStructuralChange(); RequireEntity(world,root); const auto snapshot=ValidateHierarchy(world);
		std::vector<std::size_t> work{FindNode(snapshot,root)};
		for (std::size_t i=0; i<work.size(); ++i)
			for (auto child:snapshot.nodes[work[i]].children) work.push_back(child);
		for (auto it=work.rbegin(); it!=work.rend(); ++it) world.DestroyEntity(snapshot.nodes[*it].entity);
	}
}
