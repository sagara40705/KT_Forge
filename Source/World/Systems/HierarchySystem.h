#pragma once
#include <World/Scene/SceneUpdateContext.h>
#include <span>

namespace KT::World
{
	// 捕捉時に検証済みの親子索引と階層順序を、CPU計算で使用可能にする。
	class HierarchySystem
	{
	public:
		void Update(SceneUpdateContext& context) const;

	private:
		friend class World;
		// Worldの派生値を全件検証して作る。完成後はconstとして共有する。
		static HierarchySnapshot Build(std::span<const HierarchyInput> inputs);
	};
}
