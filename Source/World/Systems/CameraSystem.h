#pragma once
#include <World/Scene/SceneUpdateContext.h>

namespace KT::World
{
	// 指定された1台のCameraと有効状態・World行列から、viewとprojectionを求める。
	// 指定が不正な場合は、別のCameraへ切り替えない。
	class CameraSystem
	{
	public:
		void Update(SceneUpdateContext& context) const;
	};
}
