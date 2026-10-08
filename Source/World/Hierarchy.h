#pragma once
#include <World/World.h>
#include <World/SceneComponents.h>
#include <limits>
#include <vector>

namespace KT::World
{
	inline constexpr std::size_t NoParent = (std::numeric_limits<std::size_t>::max)();
	struct HierarchyNode
	{
		Entity entity;
		std::size_t parent = NoParent;
		std::vector<std::size_t> children;
	};
	// 値snapshot。親正本はHierarchy componentだけ。この派生indexはECSへ保存しない。
	struct HierarchySnapshot
	{
		std::vector<HierarchyNode> nodes;
		std::vector<std::size_t> parentFirst;
	};
	HierarchySnapshot ValidateHierarchy(const World& world);
	void SetParent(World& world, Entity child, Entity parent = {}); // KeepLocalのみ。検証後commit。
	Entity GetParent(const World& world, Entity child);
	std::vector<Entity> GetChildren(const World& world, Entity parent);
	void DestroySubtree(World& world, Entity root); // 全検証/作業領域確保後、子から破棄。
}
