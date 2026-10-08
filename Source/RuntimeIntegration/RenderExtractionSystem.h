#pragma once
#include <RuntimeIntegration/RenderFrame.h>
#include <World/Systems/SceneUpdateContext.h>

namespace KT::RuntimeIntegration
{
	// 完成CPU contextからRenderer用の値をコピーする。他Systemの実行やWorldの再読取は行わない。
	class RenderExtractionSystem
	{
	public:
		RenderFrame Extract(const KT::World::SceneUpdateContext& context) const;
	};
}
