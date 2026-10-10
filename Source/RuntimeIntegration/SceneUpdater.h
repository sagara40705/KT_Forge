#pragma once
#include <RuntimeIntegration/RenderExtractionSystem.h>
#include <Core/Utility/NonCopyable.h>

namespace KT::RuntimeIntegration
{
	// 完成CPU結果からCameraと描画用の値を計算する。World更新はSceneが担当する。
	// 失敗時は旧/途中Frameを公開しない。戻り参照は次Updateまで。保持には値コピーを使う。
	class SceneUpdater : private KT::Core::NonCopyable
	{
	public:
		const RenderFrame& Update(
			const KT::World::SceneUpdateContext& cpu, std::optional<KT::World::Entity> camera, KT::World::Viewport viewport);
		const RenderFrame& Get() const;

		bool HasFrame() const noexcept
		{
			return frame_.has_value();
		}

	private:
		// Camera計算と描画抽出が全て成功したFrameだけを所有する。
		std::optional<RenderFrame> frame_;
	};
}
