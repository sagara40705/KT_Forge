#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Core/Timer.h>
#include <Platform/GlfwContext.h>
#include <Platform/Window.h>
#include <Platform/Input/Input.h>
#include <Graphics/GraphicsDevice.h>
#include <Graphics/Commands/CommandQueue.h>
#include <Graphics/Presentation/Swapchain.h>
#include <World/Scene/SceneHost.h>
#include <functional>
#include <memory>

namespace KT::Application
{
	// Window・入力・SceneHostを所有し、メインスレッドでCPU更新を進める。
	class Application : private KT::Core::NonCopyable
	{
	public:
		// Scriptの後に呼ぶゲーム処理。同じ回の入力と固定CPU結果を呼出中だけ借用する。
		using GameUpdate = std::function<void(KT::World::Scene&, const KT::World::SceneUpdateContext&,
			const KT::Platform::InputSnapshot&, double)>;

		// コンストラクタ(幅・高さ・タイトル)
		Application(int width, int height, const char* title);

		// 約60Hzの可変deltaでCPU更新する。最小化中も継続し、例外時はRunを終了して再送出する。
		// Run終了後の完成CPU結果はGetScene()->GetSnapshot()で取得する。callbackとRunへ再入しない。
		void Run(GameUpdate gameUpdate = {});

		// Loaderで検証した候補を所有し、次のCPU更新開始時に切り替える。
		void QueueScene(std::unique_ptr<KT::World::Scene> scene);
		// 初回更新前はnullptr。借用は次のScene切替・Application破棄まで。
		// SceneがFailedならResetAfterFailureと修復を明示するか、新しい候補を予約してRunを再開する。
		[[nodiscard]] KT::World::Scene* GetScene() noexcept;
		[[nodiscard]] const KT::World::Scene* GetScene() const noexcept;
		// ゲーム処理から終了を予約する。次の更新を始めずにRunから戻る。
		void RequestStop() noexcept;

	private:
		// 最初に有効な画面寸法が得られたとき、既存のClearを一度だけ表示する。
		void PresentInitialClear(int width, int height);

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
		// GPU初期表示の完了を保持する。作成・記録失敗後は再開時に初期表示をやり直す。
		bool initialClearPresented_ = false;

		// Sceneを入力・Graphics基盤より先に破棄し、Script側の借用先を保持する。
		KT::World::SceneHost sceneHost_;
		// Run開始時に計測を再設定し、起動・復旧待ちの時間をdeltaへ含めない。
		KT::Core::Timer timer_;
		// 同じApplicationのRunへの再入を拒否する。
		bool running_ = false;
		// CPU更新中の終了要求を、次の更新開始前に反映する。
		bool stopRequested_ = false;
	};
}
