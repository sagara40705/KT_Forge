#pragma once
#include <RuntimeIntegration/RenderExtractionSystem.h>
#include <Core/Utility/NonCopyable.h>
#include <RuntimeIntegration/Animation/AnimationExtraction.h>

namespace KT::RuntimeIntegration
{
	// 完成CPU結果から静的/Skin Mesh候補を公開し、指定時だけCameraを計算する。
	// 戻り参照は次Updateまで。保持には値コピーを使い、素材Catalogを利用完了まで保持する。
	class SceneUpdater : private KT::Core::NonCopyable
	{
	public:
		// Catalogを借用する。登録・Freezeを済ませてからUpdateを呼ぶ。
		explicit SceneUpdater(const KT::Content::AssetCatalog& assets);
		// 候補生成失敗時はFrameなし。Camera失敗時は今回の候補と診断を保持し、viewを空にする。
		const RenderFrame& Update(
			const KT::World::SceneUpdateContext& cpu, std::optional<KT::World::Entity> camera = {}, KT::World::Viewport viewport = {});
		const RenderFrame& Get() const;
		// Scene更新開始時に旧候補を失効させ、CPU更新失敗後の誤利用を防ぐ。
		void Reset() noexcept;

		bool HasFrame() const noexcept
		{
			return frame_.has_value();
		}

	private:
		// Applicationなどの所有者が、SceneUpdaterと候補利用者より長く保持する。
		const KT::Content::AssetCatalog& assets_;
		// 候補生成が完了したFrameを所有する。Cameraは独立した任意の派生結果。
		std::optional<RenderFrame> frame_;
		// 姿勢評価用cacheだけを所有し、ゲームの再生状態は保持しない。
		AnimationExtraction animation_;
	};
}
