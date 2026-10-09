#pragma once
#include <World/Systems/SceneUpdateContext.h>
#include <span>
#include <utility>

namespace KT::World
{
	// 捕捉済みの親入力を検証し、子のindexと非再帰の親先行順をcontextへ返す。
	class HierarchySystem
	{
	public:
		void Update(SceneUpdateContext& context) const;

	private:
		// 編集helperも同じ検証を共有する。生きたWorld入力へのcommitはhelper側で行う。
		friend HierarchySnapshot ValidateHierarchy(const World& world);
		friend void SetParent(World& world, Entity child, Entity parent);
		static HierarchySnapshot Build(std::span<const SceneEntityInput> inputs, std::optional<std::pair<Entity, Entity>> replacement = {});
	};
}
