#pragma once
#include <RuntimeIntegration/RenderFrame.h>
#include <World/Scene/SceneUpdateContext.h>

namespace KT::RuntimeIntegration
{
	// 完成CPU contextからRenderer用の値をコピーする。他Systemの実行やWorldの再読取は行わない。
	class RenderExtractionSystem
	{
	public:
		RenderFrame Extract(const KT::World::SceneUpdateContext& context) const;

	private:
		friend class SceneUpdater;
		// SceneUpdaterが同じCPU入力から計算したCamera結果だけを受け取る。
		RenderFrame Extract(const KT::World::SceneUpdateContext& context, const std::optional<KT::World::CameraViewData>& camera) const;
	};
}
