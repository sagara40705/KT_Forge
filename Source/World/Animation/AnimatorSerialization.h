#pragma once
#include <World/Scene/ComponentRegistry.h>

namespace KT::World
{
	// 再生設定だけを保存し、現在時刻・遷移途中・ozz cacheはロード時に作り直す。
	void RegisterAnimatorComponent(ComponentRegistry& registry);
}
