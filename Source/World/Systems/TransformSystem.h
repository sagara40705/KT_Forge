#pragma once
#include <World/Scene/SceneUpdateContext.h>

namespace KT::World
{
	// ローカルTRSと階層から、無効なEntityも含めてWorld行列を求める。
	// shearを保つため、結果をTRSへ再分解しない。
	class TransformSystem
	{
	public:
		void Update(SceneUpdateContext& context) const;
	};
}
