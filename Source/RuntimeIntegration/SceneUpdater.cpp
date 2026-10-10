#include <RuntimeIntegration/SceneUpdater.h>
#include <World/Systems/CameraSystem.h>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace KT::RuntimeIntegration
{
	const RenderFrame& SceneUpdater::Update(
		const KT::World::SceneUpdateContext& cpu, std::optional<KT::World::Entity> camera, KT::World::Viewport viewport)
	{
		// 前回の公開結果を失効させ、失敗時に古いFrameを返さない。
		frame_.reset();

		// Sceneが公開した同じCPU結果を利用し、Worldを再読取しない。
		const auto view = KT::World::CameraSystem{}.Calculate(cpu, camera, viewport);
		auto pending = RenderExtractionSystem{}.Extract(cpu, view);

		// 全計算が成功したFrameだけを公開する。
		static_assert(std::is_nothrow_move_constructible_v<RenderFrame>);
		frame_.emplace(std::move(pending));
		return *frame_;
	}

	const RenderFrame& SceneUpdater::Get() const
	{
		if (!frame_)
		{
			throw std::logic_error("完成したRenderFrameがありません。");
		}
		return *frame_;
	}
}
