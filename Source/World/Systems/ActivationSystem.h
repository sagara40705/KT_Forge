#pragma once
#include <World/Systems/SceneUpdateContext.h>

namespace KT::World
{
	// 検証済み階層とActiveSelfから親継承activeを導出する。ActiveSelfへ書き戻さない。
	class ActivationSystem
	{
	public:
		void Update(SceneUpdateContext& context) const;
	};
}
