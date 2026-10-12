#include <RuntimeIntegration/SceneUpdater.h>
#include <World/Systems/CameraSystem.h>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace KT::RuntimeIntegration
{
	SceneUpdater::SceneUpdater(const KT::Content::AssetCatalog& assets)
		: assets_(assets)
	{
	}

	const RenderFrame& SceneUpdater::Update(
		const KT::World::SceneUpdateContext& cpu, std::optional<KT::World::Entity> camera, KT::World::Viewport viewport,
		bool calculateSkinnedBounds)
	{
		// 捕捉の失敗・未完成では、同じcontextの古い候補も公開しない。
		if (cpu.CurrentStage() < KT::World::SceneUpdateContext::Stage::Transform ||
			cpu.CurrentStage() == KT::World::SceneUpdateContext::Stage::Failed)
		{
			Reset();
			(void)cpu.GetTransforms();
		}

		// 固定Catalogと同じ完成捕捉だけを再利用し、新しい候補の失敗ではFrameを失効させる。
		if (!frame_ || sourceCaptureId_ != cpu.CaptureId() || skinnedBounds_ != calculateSkinnedBounds)
		{
			Reset();
			auto pending = RenderExtractionSystem{}.Extract(cpu, assets_);
			animation_.Apply(cpu, assets_, pending, calculateSkinnedBounds);
			static_assert(std::is_nothrow_move_constructible_v<RenderFrame>);
			frame_.emplace(std::move(pending));
			sourceCaptureId_ = cpu.CaptureId();
			skinnedBounds_ = calculateSkinnedBounds;
		}

		// Cameraの失敗で完成候補を失効させず、前回のviewだけを必ず消す。
		frame_->view.reset();
		const auto view = KT::World::CameraSystem{}.Calculate(cpu, camera, viewport);
		if (view)
		{
			frame_->view = KT::Renderer::RenderView{
				view->view, view->projection, view->viewProjection, view->position, view->viewport.width, view->viewport.height};
		}
		return *frame_;
	}

	void SceneUpdater::Reset() noexcept
	{
		frame_.reset();
		sourceCaptureId_ = 0;
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
