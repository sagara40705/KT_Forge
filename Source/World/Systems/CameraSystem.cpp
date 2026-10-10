#include <World/Systems/CameraSystem.h>
#include <Core/Math/Matrix4.h>
#include <stdexcept>

namespace KT::World
{
	std::optional<CameraViewData> CameraSystem::Calculate(
		const SceneUpdateContext& context, std::optional<Entity> cameraEntity, Viewport viewport) const
	{
		context.RequireAtLeast(SceneUpdateContext::Stage::Transform);
		std::optional<CameraViewData> cameraView;
		if (cameraEntity)
		{
			// 捕捉済みの索引でCameraのWorld・slot・世代を照合する。
			const auto cameraIndex = context.hierarchy_->FindNode(*cameraEntity);

			// Cameraの生存・有効状態と、描画に必要な入力を検査する。
			if (!context.active_[cameraIndex])
			{
				throw std::invalid_argument("指定したCameraのEntityが捕捉結果にないか、無効状態です。");
			}

			const auto& camera = context.inputs_[cameraIndex].camera;
			if (!camera || viewport.width == 0 || viewport.height == 0)
			{
				throw std::invalid_argument("指定EntityにCamera componentがないか、Viewportの幅または高さが0です。");
			}

			// Cameraと祖先の拡縮を拒否し、viewの前提をそろえる。
			for (auto ancestorIndex = cameraIndex; ancestorIndex != NoParent;
				ancestorIndex = context.hierarchy_->nodes[ancestorIndex].parent)
			{
				const auto scale = context.inputs_[ancestorIndex].local.scale;
				if (scale.x != 1 || scale.y != 1 || scale.z != 1)
				{
					throw std::invalid_argument("Cameraとその祖先のLocalTransformのscaleは全軸1である必要があります。");
				}
			}

			// World行列の逆行列とReverse-Z投影からCamera値を組み立てる。
			const auto& matrix = context.frame_.entities[cameraIndex].transform.matrix;
			const auto view = KT::Core::Math::Inverse(matrix);
			const auto projection = KT::Core::Math::PerspectiveReverseZ(
				camera->verticalFovRadians, float(viewport.width) / float(viewport.height), camera->nearPlane, camera->farPlane);
			cameraView = CameraViewData{*cameraEntity, view, projection, KT::Core::Math::Multiply(view, projection),
				{matrix(3, 0), matrix(3, 1), matrix(3, 2)}, viewport};
		}

		return cameraView;
	}

	void CameraSystem::Update(SceneUpdateContext& context) const
	{
		// CPU計算の完了後にCameraを追加し、失敗したcontextは続行させない。
		context.RequireStage(SceneUpdateContext::Stage::Transform);
		try
		{
			context.frame_.camera = Calculate(context, context.camera_, context.viewport_);
			context.stage_ = SceneUpdateContext::Stage::Camera;
		}
		catch (...)
		{
			context.Fail();
			throw;
		}
	}
}
