#pragma once
#include <World/Scene/SceneUpdateContext.h>
#include <span>
#include <utility>

namespace KT::World
{
	// 捕捉済みの親を検証し、子のindexと親から処理する順序をcontextへ返す。
	class HierarchySystem
	{
	public:
		void Update(SceneUpdateContext& context) const;

	private:
		// 親編集と同じ検証を共有する。Worldへの反映は編集関数で行う。
		friend HierarchySnapshot ValidateHierarchy(const World& world);
		friend void SetParent(World& world, Entity child, Entity parent);
		static HierarchySnapshot Build(std::span<const SceneEntityInput> inputs, std::optional<std::pair<Entity, Entity>> replacement = {});
	};
}
