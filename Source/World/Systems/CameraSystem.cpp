#include <World/Systems/CameraSystem.h>
#include <Core/Math/Matrix4.h>
#include <stdexcept>

namespace KT::World
{
	void CameraSystem::Update(SceneUpdateContext& context) const
	{
		context.RequireStage(SceneUpdateContext::Stage::Transform);
		try
		{
			std::optional<CameraViewData> result;
			if (context.camera_)
			{
				std::size_t index = NoParent;
				for (std::size_t i = 0; i < context.inputs_.size(); ++i)
				{
					if (context.inputs_[i].entity == *context.camera_)
					{
						index = i;
					}
				}
				if (index == NoParent || !context.active_[index])
				{
					throw std::invalid_argument("Selected camera is stale, foreign or inactive.");
				}
				const auto& camera = context.inputs_[index].camera;
				if (!camera || context.viewport_.width == 0 || context.viewport_.height == 0)
				{
					throw std::invalid_argument("Camera component or viewport is invalid.");
				}
				for (auto i = index; i != NoParent; i = context.hierarchy_.nodes[i].parent)
				{
					const auto scale = context.inputs_[i].local.scale;
					if (scale.x != 1 || scale.y != 1 || scale.z != 1)
					{
						throw std::invalid_argument("Camera and ancestors must have unit local scale.");
					}
				}
				const auto& matrix = context.frame_.entities[index].transform.matrix;
				const auto view = KT::Core::Math::Inverse(matrix);
				const auto projection = KT::Core::Math::PerspectiveReverseZ(camera->verticalFovRadians,
					float(context.viewport_.width) / float(context.viewport_.height), camera->nearPlane, camera->farPlane);
				result = CameraViewData{*context.camera_, view, projection, KT::Core::Math::Multiply(view, projection),
					{matrix(3, 0), matrix(3, 1), matrix(3, 2)}, context.viewport_};
			}
			context.frame_.camera = result;
			context.stage_ = SceneUpdateContext::Stage::Camera;
		}
		catch (...)
		{
			context.Fail();
			throw;
		}
	}
}
