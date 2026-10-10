#pragma once
#include <World/World.h>
#include <World/Scene/SceneComponents.h>
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

	// 階層を値として保持する。親の正本はHierarchy componentとし、派生indexはECSへ保存しない。
	struct HierarchySnapshot
	{
		std::vector<HierarchyNode> nodes;
		std::vector<std::size_t> parentFirst;
	};

	HierarchySnapshot ValidateHierarchy(const World& world);
	// ローカルTransformを保ち、階層全体の検証後に親を変更する。
	void SetParent(World& world, Entity child, Entity parent = {});
	Entity GetParent(const World& world, Entity child);
	std::vector<Entity> GetChildren(const World& world, Entity parent);
	// 全件検証と作業領域の確保を済ませ、子から順に破棄する。
	void DestroySubtree(World& world, Entity root);
}
