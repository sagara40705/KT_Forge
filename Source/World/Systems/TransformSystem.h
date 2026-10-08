#pragma once
#include <World/Systems/SceneUpdateContext.h>

namespace KT::World
{
	// Local TRSと階層から非activeも含む全World行列を導出する。shearを保つためTRSへ再分解しない。
	class TransformSystem
	{
	public:
		void Update(SceneUpdateContext& context) const;
	};
}
