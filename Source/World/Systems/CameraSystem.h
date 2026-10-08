#pragma once
#include <World/Systems/SceneUpdateContext.h>

namespace KT::World
{
	// 明示1台のCamera、active、World行列からview/projectionを導出する。代替Cameraは選ばない。
	class CameraSystem
	{
	public:
		void Update(SceneUpdateContext& context) const;
	};
}
