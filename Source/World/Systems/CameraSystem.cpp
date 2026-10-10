#include <World/Systems/CameraSystem.h>
#include <Core/Math/Matrix4.h>
#include <stdexcept>

namespace KT::World
{
	void CameraSystem::Update(SceneUpdateContext& context) const
	{
		// 前段階の完了を確認し、同じ更新の結果だけを使う。
		context.RequireStage(SceneUpdateContext::Stage::Transform);

		try
		{
			std::optional<CameraViewData> cameraView;
			if (context.camera_)
			{
				// 指定Cameraを捕捉済みの入力から探す。
				std::size_t cameraIndex = NoParent;
				for (std::size_t inputIndex = 0; inputIndex < context.inputs_.size(); ++inputIndex)
				{
					if (context.inputs_[inputIndex].entity == *context.camera_)
					{
						cameraIndex = inputIndex;
					}
				}

				// Cameraの生存・有効状態と、描画に必要な入力を検査する。
				if (cameraIndex == NoParent || !context.active_[cameraIndex])
				{
					throw std::invalid_argument("Selected camera is stale, foreign or inactive.");
				}

				const auto& camera = context.inputs_[cameraIndex].camera;
				if (!camera || context.viewport_.width == 0 || context.viewport_.height == 0)
				{
					throw std::invalid_argument("Camera component or viewport is invalid.");
				}

				// Cameraと祖先の拡縮を拒否し、viewの前提をそろえる。
				for (auto ancestorIndex = cameraIndex; ancestorIndex != NoParent;
					ancestorIndex = context.hierarchy_.nodes[ancestorIndex].parent)
				{
					const auto scale = context.inputs_[ancestorIndex].local.scale;
					if (scale.x != 1 || scale.y != 1 || scale.z != 1)
					{
						throw std::invalid_argument("Camera and ancestors must have unit local scale.");
					}
				}

				// World行列の逆行列とReverse-Z投影からCamera値を組み立てる。
				const auto& matrix = context.frame_.entities[cameraIndex].transform.matrix;
				const auto view = KT::Core::Math::Inverse(matrix);
				const auto projection = KT::Core::Math::PerspectiveReverseZ(camera->verticalFovRadians,
					float(context.viewport_.width) / float(context.viewport_.height), camera->nearPlane, camera->farPlane);
				cameraView = CameraViewData{*context.camera_, view, projection, KT::Core::Math::Multiply(view, projection),
					{matrix(3, 0), matrix(3, 1), matrix(3, 2)}, context.viewport_};
			}

			// 未選択の場合も空のCamera結果として更新を完了する。
			context.frame_.camera = cameraView;
			context.stage_ = SceneUpdateContext::Stage::Camera;
		}
		catch (...)
		{
			// 失敗した更新の続行を禁止し、元の例外を返す。
			context.Fail();
			throw;
		}
	}
}
