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
		const KT::World::SceneUpdateContext& cpu, std::optional<KT::World::Entity> camera, KT::World::Viewport viewport)
	{
		// 前回の公開結果を失効させ、失敗時に古いFrameを返さない。
		frame_.reset();

		// Cameraなしでも候補を完成させる。以後のCamera失敗はこのCPU結果を失効させない。
		auto pending = RenderExtractionSystem{}.Extract(cpu, assets_);
		animation_.Apply(cpu, assets_, pending);

		// 完成した候補だけを公開し、任意のCamera結果を同じ入力から追加する。
		static_assert(std::is_nothrow_move_constructible_v<RenderFrame>);
		frame_.emplace(std::move(pending));
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
