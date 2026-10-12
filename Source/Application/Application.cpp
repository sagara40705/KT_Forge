#include "Application.h"
#include <Core/Log.h>
#include <Graphics/Textures/ColorTargetView.h>
#include <RuntimeIntegration/SceneCamera.h>
#include <RuntimeIntegration/Physics/JoltScenePhysics.h>
#include <World/Scene/SceneLoader.h>
#include <BuiltinAssets/StaticMeshAssets.h>
#include <algorithm>
#include <chrono>
#include <exception>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>

namespace KT::Application
{
	Application::Application(
		int width, int height, const char* title, AssetSetup assetSetup, std::optional<ProjectStartup> project, SceneFactory sceneFactory)
		: window_(width, height, title),
		  input_(window_),
		  graphicsDevice_(),
		  commandQueue_(graphicsDevice_),
		  sceneUpdater_(assets_)
	{
		if (project && sceneFactory)
		{
			throw std::invalid_argument("Applicationの起動SceneはprojectとSceneFactoryを同時に指定できません。");
		}
		// 素材の登録を起動時に完了し、Scene使用中の差替えや参照失効を防ぐ。
		KT::BuiltinAssets::RegisterStaticMeshAssets(assets_);
		if (assetSetup)
		{
			assetSetup(assets_);
		}
		if (project)
		{
			SetRenderCamera(project->camera);
			KT::World::ComponentRegistry components;
			KT::World::RegisterEngineComponents(components);
			KT::World::ScriptRegistry scripts;
			if (project->registerTypes)
			{
				project->registerTypes(components, scripts);
			}
			components.Freeze();
			scripts.Freeze();
			const auto index = KT::Content::ProjectAssets::Scan(project->root);
			auto loaded = KT::RuntimeIntegration::ProjectSceneLoader::Load(index, project->scene, assets_, components, scripts);
			assetDiagnostics_ = std::move(loaded.diagnostics);
			QueueScene(std::move(loaded.scene));
		}
		else
		{
			// 起動Sceneの方針は呼出側が選ぶ。指定がなければ空Sceneを使う。
			QueueScene(sceneFactory ? sceneFactory() : KT::World::SceneLoader::Load(KT::World::SceneAsset{}));
		}
		assets_.Freeze();
	}

	Application::~Application()
	{
		try
		{
			Shutdown();
		}
		catch (...)
		{
			// GPU完了を確認できないまま借用資源を解放することはできない。
			KT_LOG_ERROR("Applicationの終了処理に失敗しました。安全に資源を破棄できないため終了します。");
			std::terminate();
		}
	}

	void Application::RequireAvailable() const
	{
		if (shuttingDown_ || runState_ == RunState::Shutdown)
		{
			throw std::logic_error("Applicationは終了処理中または終了済みのため、操作できません。");
		}
	}

	void Application::Shutdown()
	{
		if (runState_ == RunState::Shutdown)
		{
			return;
		}
		RequireAvailable();
		if (runState_ == RunState::Running)
		{
			throw std::logic_error("Run中はApplicationを終了できません。RequestStop後、Runから戻って終了してください。");
		}

		// GPU借用を解消するまではSceneも公開窓口も保持する。
		shuttingDown_ = true;
		try
		{
			if (renderer_)
			{
				renderer_->ReleaseFrame();
			}
			if (swapchain_ || effectPlayer_ || uiRenderer_)
			{
				commandQueue_.Wait(commandQueue_.Signal());
			}
			sceneUpdater_.Reset();
			sceneHost_.Clear();
		}
		catch (...)
		{
			lastRunFailure_ = std::current_exception();
			runState_ = RunState::Failed;
			shuttingDown_ = false;
			throw;
		}

		// Scriptの破棄を終えてから窓口とその借用先を順に解放する。
		renderer_.reset();
		ui_.reset();
		uiRenderer_.reset();
		effects_.reset();
		effectPlayer_.reset();
		swapchain_.reset();
		execution_.Stop();
		runState_ = RunState::Shutdown;
		shuttingDown_ = false;
	}

	void Application::Play()
	{
		RequireAvailable();
		execution_.Play();
	}

	void Application::Pause()
	{
		RequireAvailable();
		execution_.Pause();
	}

	void Application::StopPlayback()
	{
		RequireAvailable();
		execution_.Stop();
	}

	void Application::RequestSingleStep()
	{
		RequireAvailable();
		execution_.RequestSingleStep();
	}

	ExecutionState Application::GetExecutionState() const noexcept
	{
		return execution_.State();
	}

	Application::RunState Application::GetRunState() const noexcept
	{
		return runState_;
	}

	std::exception_ptr Application::GetLastRunFailure() const noexcept
	{
		return lastRunFailure_;
	}

	void Application::QueueScene(std::unique_ptr<KT::World::Scene> scene)
	{
		RequireAvailable();
		if (!scene)
		{
			throw std::invalid_argument("Applicationへ空のSceneを予約できません。");
		}
		// 候補の物理準備まで完了してから予約する。失敗時は現在Sceneを保持する。
		KT::RuntimeIntegration::PrepareScenePhysics(*scene, physicsRuntime_);
		sceneHost_.QueueScene(std::move(scene));
	}

	KT::World::Scene* Application::GetScene() noexcept
	{
		return sceneHost_.GetScene();
	}

	const KT::World::Scene* Application::GetScene() const noexcept
	{
		return sceneHost_.GetScene();
	}

	const KT::Content::AssetCatalog& Application::GetAssets() const noexcept
	{
		return assets_;
	}

	std::span<const KT::RuntimeIntegration::SceneAssetDiagnostic> Application::GetAssetDiagnostics() const noexcept
	{
		return assetDiagnostics_;
	}

	const KT::RuntimeIntegration::RenderFrame* Application::GetRenderFrame() const
	{
		const auto* scene = sceneHost_.GetScene();
		if (!scene || !scene->HasSnapshot() || !sceneUpdater_.HasFrame())
		{
			return nullptr;
		}
		const auto& frame = sceneUpdater_.Get();
		const auto& snapshot = scene->GetSnapshot();
		if (frame.sourceWorldId != snapshot.SourceWorldId() || frame.updateNumber != snapshot.UpdateNumber())
		{
			return nullptr;
		}
		return &frame;
	}

	KT::RuntimeIntegration::SceneEffects& Application::Effects()
	{
		RequireAvailable();
		if (!effects_)
		{
			auto player = std::make_unique<KT::Renderer::EffectPlayer>(graphicsDevice_, commandQueue_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
			auto effects = std::make_unique<KT::RuntimeIntegration::SceneEffects>(*player);
			if (effectsTick_)
			{
				effects->BeginTick();
			}
			effectPlayer_ = std::move(player);
			effects_ = std::move(effects);
		}
		return *effects_;
	}

	KT::RuntimeIntegration::GameUi& Application::UI()
	{
		RequireAvailable();
		if (!ui_)
		{
			if (runState_ == RunState::Running)
			{
				throw std::logic_error("UIはRun前に取得し、素材を準備してください。");
			}
			auto renderer = std::make_unique<KT::Renderer::UiRenderer>(graphicsDevice_, commandQueue_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
			auto ui = std::make_unique<KT::RuntimeIntegration::GameUi>(*renderer);
			ui->SetFrameKeys(frameKeys_);
			uiRenderer_ = std::move(renderer);
			ui_ = std::move(ui);
		}
		return *ui_;
	}

	void Application::SetFrameKeys(std::span<const KT::Platform::Key> keys)
	{
		RequireAvailable();
		if (runState_ == RunState::Running)
		{
			throw std::logic_error("外側フレームの予約キーはRun前に設定してください。");
		}
		std::vector<KT::Platform::Key> candidate(keys.begin(), keys.end());
		frameInput_.SetFrameKeys(candidate);
		if (ui_)
		{
			ui_->SetFrameKeys(candidate);
		}
		frameKeys_.swap(candidate);
	}

	const KT::Platform::InputSnapshot& Application::FrameInput() const noexcept
	{
		return ui_ ? ui_->FrameInput() : frameInput_.FrameInput();
	}

	void Application::RequestStop() noexcept
	{
		stopRequested_ = true;
	}

	void Application::SetRenderCamera(std::optional<KT::World::ObjectUuid> camera)
	{
		RequireAvailable();
		if (camera && !camera->IsValid())
		{
			throw std::invalid_argument("描画Cameraには有効なUUIDを指定してください。");
		}
		renderCamera_ = camera;
	}

	const Application::PresentationStatistics& Application::GetPresentationStatistics() const noexcept
	{
		return presentation_;
	}

	void Application::Run(GameUpdate gameUpdate, UiUpdate uiUpdate, FrameUpdate frameUpdate)
	{
		RequireAvailable();
		if (runState_ == RunState::Running)
		{
			throw std::logic_error("ApplicationのRunが実行中のため、再入できません。");
		}

		// callbackを一度だけ接続し、SceneのScript後に今回の入力を渡す。
		KT::World::Scene::GameUpdate update;
		if (gameUpdate)
		{
			update = [this, &gameUpdate](KT::World::Scene& scene, const KT::World::SceneUpdateContext& cpu, double deltaSeconds)
			{ gameUpdate(scene, cpu, ui_ ? ui_->GameInput() : frameInput_.GameInput(), deltaSeconds); };
		}

		runState_ = RunState::Running;
		lastRunFailure_ = {};
		stopRequested_ = false;
		timer_.Reset();
		execution_.ResetClock();
		try
		{
			while (!stopRequested_ && !window_.ShouldClose())
			{
				const auto updateStarted = std::chrono::steady_clock::now();

				// イベント待機でCPUを止めず、閉じる要求を受けた回は更新しない。
				glfwContext_.PollEvents();
				if (stopRequested_ || window_.ShouldClose())
				{
					break;
				}

				// 0tickの回は入力edgeを保留し、追いつき中は最初のtickだけで消費する。
				const auto elapsed = timer_.Tick();
				int width = 0;
				int height = 0;
				bool drawable = false;
				input_.Update();
				try
				{
					if (ui_)
					{
						// UIは描画ごとに入力を処理し、ポーズ中も操作とanimationを継続する。
						int contentWidth = 0;
						int contentHeight = 0;
						window_.GetContentSize(contentWidth, contentHeight);
						window_.GetFramebufferSize(width, height);
						ui_->Update(input_.GetSnapshot(), contentWidth, contentHeight, static_cast<unsigned>((std::max)(width, 0)),
							static_cast<unsigned>((std::max)(height, 0)));
						if (uiUpdate)
						{
							uiUpdate(*ui_, elapsed);
						}
					}
					else
					{
						frameInput_.Route(nullptr, input_.GetSnapshot(), 0, 0, false);
					}
					if (frameUpdate)
					{
						frameUpdate(*this, elapsed);
					}
					if (stopRequested_ || window_.ShouldClose())
					{
						break;
					}
					// 初回表示は時間を進めずに所有だけを交換する。停止中の通常切替は保留する。
					if (!sceneHost_.GetScene() && sceneHost_.HasQueuedScene())
					{
						sceneHost_.ActivateQueuedScene();
					}
					const auto advance = execution_.Advance(elapsed);
					for (unsigned tick = 0; tick < advance.ticks && !stopRequested_; ++tick)
					{
						// callback中のポーズ・停止は現在tick完了後、次tickの前に反映する。
						if (execution_.State() != ExecutionState::Playing &&
							!(advance.singleStep && tick == 0 && execution_.State() == ExecutionState::Paused))
						{
							break;
						}
						if (simulationTicks_ == (std::numeric_limits<std::uint64_t>::max)())
						{
							throw std::overflow_error("Applicationのsimulation tick数が上限に達しました。");
						}
						sceneUpdater_.Reset();
						// ゲーム中の再生予約はScene成功後だけ公開する。
						if (effects_)
						{
							effects_->BeginTick();
						}
						effectsTick_ = true;
						try
						{
							(void)sceneHost_.Update(KT::Physics::FixedDeltaSeconds, update);
							effectsTick_ = false;
							if (effects_)
							{
								effects_->CompleteTick(sceneHost_.GetScene()->GetSnapshot(), KT::Physics::FixedDeltaSeconds);
							}
						}
						catch (...)
						{
							effectsTick_ = false;
							if (effects_)
							{
								effects_->AbortTick();
							}
							throw;
						}
						++simulationTicks_;
						if (ui_)
						{
							ui_->ConsumeGameInput();
						}
						else
						{
							frameInput_.Consume();
						}
					}
					const auto* scene = sceneHost_.GetScene();
					// 停止中のゲームedgeを再開へ蓄積しない。予約キーはFrameInputで処理済み。
					if (execution_.State() != ExecutionState::Playing)
					{
						if (ui_)
						{
							ui_->ConsumeGameInput();
						}
						else
						{
							frameInput_.Consume();
						}
					}
					window_.GetFramebufferSize(width, height);
					drawable = scene && width > 0 && height > 0 && !window_.IsMinimized();
					// 最小化中もCPU候補を更新する。描画する回だけCameraとviewportを評価する。
					const auto camera =
						drawable ? KT::RuntimeIntegration::SelectSceneCamera(*sceneHost_.GetScene(), renderCamera_) : std::nullopt;
					const KT::World::Viewport viewport = drawable ?
						KT::World::Viewport{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)} :
						KT::World::Viewport{};
					if (scene)
					{
						(void)sceneUpdater_.Update(scene->GetSnapshot(), camera, viewport);
					}
				}
				catch (...)
				{
					KT_LOG_ERROR("SceneのCPU更新または描画候補生成に失敗したため、Applicationの通常更新を停止します。");
					throw;
				}

				// 成功した同じ回の候補とCameraだけを描画する。
				if (stopRequested_ || window_.ShouldClose())
				{
					break;
				}
				if (drawable)
				{
					PresentScene(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
				}

				// 描画がない間もイベントpollの空回りを約60Hzに抑える。
				std::this_thread::sleep_until(updateStarted + std::chrono::duration<double>(1.0 / 60.0));
			}
		}
		catch (...)
		{
			// 元の失敗を先に保持し、後始末の失敗でもRun中という状態を残さない。
			lastRunFailure_ = std::current_exception();
			runState_ = RunState::Failed;
			// GPUの借用を解消し、完成済みCPU状態を保持して呼出側へ失敗を返す。
			try
			{
				if (renderer_)
				{
					renderer_->ReleaseFrame();
				}
			}
			catch (...)
			{
				KT_LOG_ERROR("Run失敗後のGPU完了待機にも失敗しました。元のRun例外を呼出側へ返します。");
			}
			std::rethrow_exception(lastRunFailure_);
		}
		runState_ = RunState::Idle;
	}

	void Application::PresentScene(std::uint32_t width, std::uint32_t height)
	{
		if (presentation_.frames == (std::numeric_limits<std::uint64_t>::max)())
		{
			throw std::overflow_error("Applicationの表示回数が上限に達しました。");
		}
		bool resized = false;
		if (!swapchain_)
		{
			swapchain_ =
				std::make_unique<KT::Graphics::Swapchain>(graphicsDevice_, commandQueue_, window_.GetNativeHandle(), width, height, true);
			resized = true;
		}
		if (!renderer_)
		{
			renderer_ =
				std::make_unique<KT::Renderer::StaticSceneRenderer>(graphicsDevice_, commandQueue_, assets_, swapchain_->GetRtvFormat());
		}
		if (swapchain_->GetWidth() != width || swapchain_->GetHeight() != height)
		{
			// 完了したListにも旧画像の参照を残さず、全BackBufferを取り直す。
			renderer_->ReleaseFrame();
			swapchain_->Resize(width, height);
			resized = true;
		}
		const auto& frame = sceneUpdater_.Get();
		if (ui_)
		{
			// 上流UIのTexture転送と描画列作成はGraph/Frame開始前に完了する。
			ui_->PrepareFrame(width, height);
		}
		const auto backBufferIndex = swapchain_->GetCurrentBackBufferIndex();
		renderer_->Submit(frame.candidates, frame.view, KT::Graphics::ColorTargetView(*swapchain_, backBufferIndex),
			D3D12_RESOURCE_STATE_PRESENT, frame.lighting, effectPlayer_.get(), uiRenderer_.get());
		swapchain_->Present();
		renderer_->Wait();
		presentation_ = {
			presentation_.frames + 1, width, height, renderer_->LastDrawCount(), renderer_->MeshCount(), renderer_->MaterialCount()};
		if (resized)
		{
			// 初回GPU生成やResizeの待ち時間を次のゲームdeltaへ持ち越さない。
			timer_.Reset();
			execution_.ResetClock();
		}
	}
}
