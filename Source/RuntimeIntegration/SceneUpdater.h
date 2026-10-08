#pragma once
#include <RuntimeIntegration/RenderExtractionSystem.h>
#include <Core/Utility/NonCopyable.h>

namespace KT::RuntimeIntegration
{
	// 固定順で5 Systemを呼び、完成RenderFrameだけを所有/公開する薄い入口。
	// 失敗時は旧/途中Frameを公開しない。戻り参照は次Updateまで。保持には値コピーを使う。
	class SceneUpdater : private KT::Core::NonCopyable
	{
	public:
		const RenderFrame& Update(const KT::World::World& world,
			std::optional<KT::World::Entity> camera, KT::World::Viewport viewport);
		const RenderFrame& Get() const;
		bool HasFrame() const noexcept { return frame_.has_value(); }
	private:
		std::optional<RenderFrame> frame_;
	};
}
