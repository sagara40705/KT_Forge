#pragma once
#include <World/World.h>
#include <World/Scene/SceneComponents.h>
#include <limits>
#include <vector>

namespace KT::World
{
	inline constexpr std::size_t NoParent = (std::numeric_limits<std::size_t>::max)();

	// 階層検証に必要なEntityと親だけを捕捉し、Transform等の値と分離する。
	struct HierarchyInput
	{
		Entity entity;
		Entity parent;
	};

	// 密なノード番号で親子を参照する。Entityのslot番号とは区別する。
	struct HierarchyNode
	{
		Entity entity;
		std::size_t parent = NoParent;
		std::vector<std::size_t> children;
	};

	// 検証済みの派生索引。WorldとCPU入力がconst共有し、親の正本はHierarchy componentに置く。
	struct HierarchySnapshot
	{
		std::vector<HierarchyNode> nodes;
		// 根と兄弟はslot順。親を子より先に処理する幅優先の順序。
		std::vector<std::size_t> parentFirst;
		// slotから密なノード番号を引く。空きslotはNoParent、世代・Worldはnodesで照合する。
		std::vector<std::size_t> nodeByEntityIndex;

		// 別World・失効世代・範囲外を拒否し、対応するノード番号を定数時間で返す。
		[[nodiscard]] std::size_t FindNode(Entity entity) const;
	};

	HierarchySnapshot ValidateHierarchy(const World& world);
	// Worldと同じ失敗保持・変換維持の契約で、階層全体の検証後に親を変更する。
	void SetParent(World& world, Entity child, Entity parent = {}, ParentChangeMode mode = ParentChangeMode::KeepLocal);
	Entity GetParent(const World& world, Entity child);
	std::vector<Entity> GetChildren(const World& world, Entity parent);
	// 全件検証と作業領域の確保を済ませ、子から順に破棄する。
	void DestroySubtree(World& world, Entity root);
}
