#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Core/Timer.h>
#include <Application/FixedStepClock.h>
#include <Physics/PhysicsRuntime.h>
#include <Platform/GlfwContext.h>
#include <Platform/Window.h>
#include <Platform/Input/Input.h>
#include <Graphics/GraphicsDevice.h>
#include <Graphics/Commands/CommandQueue.h>
#include <Graphics/Presentation/Swapchain.h>
#include <World/Scene/SceneHost.h>
#include <RuntimeIntegration/SceneUpdater.h>
#include <RuntimeIntegration/ProjectSceneLoader.h>
#include <Renderer/StaticSceneRenderer.h>
#include <RuntimeIntegration/Effects/SceneEffects.h>
#include <functional>
#include <memory>

namespace KT::Application
{
	// Window・入力・SceneHost・Rendererを所有し、CPU更新後に同期1slotで描画する。
	class Application : private KT::Core::NonCopyable
	{
	public:
		// Scriptの後に呼ぶゲーム処理。同じ回の入力と固定CPU結果を呼出中だけ借用する。
		using GameUpdate =
			std::function<void(KT::World::Scene&, const KT::World::SceneUpdateContext&, const KT::Platform::InputSnapshot&, double)>;
		// 組込み素材登録後、初回Scene使用前に独自CPU素材を追加する起動処理。
		using AssetSetup = std::function<void(KT::Content::AssetCatalog&)>;
		// ファイル起動を使わない場合に、検証済みの起動Sceneを生成する。
		using SceneFactory = std::function<std::unique_ptr<KT::World::Scene>()>;

		// ファイル起動の設定。素材はroot内を探索し、Sceneにはrootからの相対パスを指定する。
		struct ProjectStartup
		{
			std::filesystem::path root;
			std::filesystem::path scene;
			// 組込み登録後に独自Component/Scriptを追加する。読込前にApplicationがFreezeする。
			std::function<void(KT::World::ComponentRegistry&, KT::World::ScriptRegistry&)> registerTypes;
			// 未指定なら有効なCameraを一台だけ自動選択する。
			std::optional<KT::World::ObjectUuid> camera;
		};

		// 最後にPresentまで成功した描画の情報。最小化中・描画失敗時には更新しない。
		struct PresentationStatistics
		{
			std::uint64_t frames = 0;
			std::uint32_t width = 0;
			std::uint32_t height = 0;
			std::size_t draws = 0;
			std::size_t meshes = 0;
			std::size_t materials = 0;
		};

		// コンストラクタ(幅・高さ・タイトル)
		Application(int width, int height, const char* title, AssetSetup assetSetup = {}, std::optional<ProjectStartup> project = {},
			SceneFactory sceneFactory = {});
		// Presentを含むQueueの完了後にGPU所有物を破棄する。完了待ちの失敗は継続不能とする。
		~Application();

		// Script・ゲーム・物理を1/60秒で更新する。1描画につき最大4tick、最小化中も継続する。
		// Run終了後の完成CPU結果はGetScene()->GetSnapshot()で取得する。callbackとRunへ再入しない。
		void Run(GameUpdate gameUpdate = {});

		// Loaderで検証した候補を所有し、次のCPU更新開始時に切り替える。
		void QueueScene(std::unique_ptr<KT::World::Scene> scene);
		// 初回更新前はnullptr。借用は次のScene切替・Application破棄まで。
		// SceneがFailedならResetAfterFailureと修復を明示するか、新しい候補を予約してRunを再開する。
		[[nodiscard]] KT::World::Scene* GetScene() noexcept;
		[[nodiscard]] const KT::World::Scene* GetScene() const noexcept;
		[[nodiscard]] const KT::Content::AssetCatalog& GetAssets() const noexcept;
		// 起動Sceneの欠落・種類違いを返す。非表示の参照も含む。
		[[nodiscard]] std::span<const KT::RuntimeIntegration::SceneAssetDiagnostic> GetAssetDiagnostics() const noexcept;
		// 現在の完成Sceneと一致する候補だけを返す。初回更新前・更新失敗・idle編集後はnullptr。
		// 借用は次Updateまで。値コピーで保持する場合も、素材利用中はApplicationを保持する。
		[[nodiscard]] const KT::RuntimeIntegration::RenderFrame* GetRenderFrame() const;
		// 次の描画から使うCameraの永続UUID。nulloptは自動選択。Scene切替後も指定を維持する。
		void SetRenderCamera(std::optional<KT::World::ObjectUuid> camera);
		[[nodiscard]] const PresentationStatistics& GetPresentationStatistics() const noexcept;

		[[nodiscard]] std::uint64_t CompletedSimulationTicks() const noexcept
		{
			return simulationTicks_;
		}

		[[nodiscard]] double DroppedSimulationSeconds() const noexcept
		{
			return fixedClock_.DroppedSeconds();
		}
		// ゲーム処理から終了を予約する。次の更新を始めずにRunから戻る。
		void RequestStop() noexcept;
		// 基本エフェクトのゲーム窓口。初回取得でDX12資源を準備し、起動時に素材を読む。
		KT::RuntimeIntegration::SceneEffects& Effects();

	private:
		// CPU結果を描画する。寸法変更時は旧CommandListを解放してからResizeする。
		void PresentScene(std::uint32_t width, std::uint32_t height);

		// GlfwContextの所有
		KT::Platform::GlfwContext glfwContext_{};

		// ウィンドウの所有
		KT::Platform::Window window_;
		// Windowを借用する。宣言順によりWindowより先に破棄する。
		KT::Platform::Input input_;

		// GraphicsDevice
		KT::Graphics::GraphicsDevice graphicsDevice_{};

		// CommandQueue
		KT::Graphics::CommandQueue commandQueue_;

		// Swapchain
		std::unique_ptr<KT::Graphics::Swapchain> swapchain_{};

		// CPU素材をSceneと候補より長く保持する。起動登録後は変更しない。
		KT::Content::AssetCatalog assets_;
		std::vector<KT::RuntimeIntegration::SceneAssetDiagnostic> assetDiagnostics_;
		// Sceneの物理worldより長くJoltの登録・実行資源を保持する。
		KT::Physics::PhysicsRuntime physicsRuntime_;
		// Sceneを入力・Graphics基盤より先に破棄し、Script側の借用先を保持する。
		KT::World::SceneHost sceneHost_;
		// 完成SceneからCameraに依存しないMesh候補と個体別Skin paletteを保持する。
		KT::RuntimeIntegration::SceneUpdater sceneUpdater_;
		std::optional<KT::World::ObjectUuid> renderCamera_;
		PresentationStatistics presentation_;
		FixedStepClock fixedClock_;
		std::uint64_t simulationTicks_ = 0;
		// 素材・Device・Queue・Swapchainより先に破棄し、GPUの借用先を保持する。
		// Effect資源はRendererのFrameより長く、Device/Queueより短く保持する。
		std::unique_ptr<KT::Renderer::EffectPlayer> effectPlayer_;
		std::unique_ptr<KT::RuntimeIntegration::SceneEffects> effects_;
		bool effectsTick_ = false;
		std::unique_ptr<KT::Renderer::StaticSceneRenderer> renderer_;
		// Run開始時に計測を再設定し、起動・復旧待ちの時間をdeltaへ含めない。
		KT::Core::Timer timer_;
		// 同じApplicationのRunへの再入を拒否する。
		bool running_ = false;
		// CPU更新中の終了要求を、次の更新開始前に反映する。
		bool stopRequested_ = false;
	};
}
