#include <RuntimeIntegration/SceneUpdater.h>
#include <World/Systems/HierarchySystem.h>
#include <World/Systems/ActivationSystem.h>
#include <World/Systems/TransformSystem.h>
#include <World/Systems/CameraSystem.h>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace KT::RuntimeIntegration
{
	const RenderFrame& SceneUpdater::Update(
		const KT::World::World& world, std::optional<KT::World::Entity> camera, KT::World::Viewport viewport)
	{
		frame_.reset();
		KT::World::SceneUpdateContext context(world, camera, viewport);
		// 入力は一度捕捉し、同じcontextを順番に渡す。順序はこの5呼出だけで読める。
		KT::World::HierarchySystem{}.Update(context);
		KT::World::ActivationSystem{}.Update(context);
		KT::World::TransformSystem{}.Update(context);
		KT::World::CameraSystem{}.Update(context);
		auto pending = RenderExtractionSystem{}.Extract(context);
		static_assert(std::is_nothrow_move_constructible_v<RenderFrame>);
		frame_.emplace(std::move(pending));
		return *frame_;
	}

	const RenderFrame& SceneUpdater::Get() const
	{
		if (!frame_)
		{
			throw std::logic_error("No complete RenderFrame is published.");
		}
		return *frame_;
	}
}
