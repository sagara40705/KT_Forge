#pragma once
#include <World/Scene/SceneUpdateContext.h>

namespace KT::World
{
	// 検証済み階層とActiveSelfから、親の有効状態を継承する。入力へ書き戻さない。
	class ActivationSystem
	{
	public:
		void Update(SceneUpdateContext& context) const;
	};
}
