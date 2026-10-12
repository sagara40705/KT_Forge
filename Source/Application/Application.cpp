#include "Application.h"
#include <Application/StartupScene.h>
#include <Core/Log.h>
#include <Graphics/Textures/ColorTargetView.h>
#include <RuntimeIntegration/SceneCamera.h>
#include <RuntimeIntegration/Physics/JoltScenePhysics.h>
#include <BuiltinAssets/StaticMeshAssets.h>
#include <algorithm>
#include <array>
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
			// 起動例も同じLoaderと製品描画経路に通し、特別な描画コードを持たない。
			QueueScene(sceneFactory ? sceneFactory() : CreateStartupScene());
		}
		assets_.Freeze();
	}

	Application::~Application()
	{
		if (renderer_)
		{
			renderer_->ReleaseFrame();
		}
		if (swapchain_)
		{
			commandQueue_.Wait(commandQueue_.Signal());
		}
	}

	void Application::QueueScene(std::unique_ptr<KT::World::Scene> scene)
	{
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
		if (!ui_)
		{
			if (running_)
			{
				throw std::logic_error("UIはRun前に取得し、素材を準備してください。");
			}
			// exe横の素材を使い、起動カレントディレクトリに依存させない。
			std::array<wchar_t, 32768> filename{};
			const auto length = GetModuleFileNameW(nullptr, filename.data(), static_cast<DWORD>(filename.size()));
			if (length == 0 || length >= filename.size())
			{
				throw std::runtime_error("UI素材の基準となる製品exeパスを取得できませんでした。");
			}
			auto renderer = std::make_unique<KT::Renderer::UiRenderer>(graphicsDevice_, commandQueue_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
			auto ui = std::make_unique<KT::RuntimeIntegration::GameUi>(*renderer);
			ui->LoadBuiltin(std::filesystem::path(filename.data()).parent_path() / "UI");
			uiRenderer_ = std::move(renderer);
			ui_ = std::move(ui);
		}
		return *ui_;
	}

	void Application::RequestStop() noexcept
	{
		stopRequested_ = true;
	}

	void Application::SetRenderCamera(std::optional<KT::World::ObjectUuid> camera)
	{
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

	void Application::Run(GameUpdate gameUpdate, UiUpdate uiUpdate)
	{
		if (running_)
		{
			throw std::logic_error("ApplicationのRunが実行中のため、再入できません。");
		}

		// callbackを一度だけ接続し、SceneのScript後に今回の入力を渡す。
		KT::World::Scene::GameUpdate update;
		if (gameUpdate)
		{
			update = [this, &gameUpdate](KT::World::Scene& scene, const KT::World::SceneUpdateContext& cpu, double deltaSeconds)
			{ gameUpdate(scene, cpu, ui_ ? ui_->GameInput() : input_.GetSnapshot(), deltaSeconds); };
		}

		running_ = true;
		stopRequested_ = false;
		timer_.Reset();
		fixedClock_.Reset();
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
				const auto ticks = fixedClock_.Advance(elapsed);
				int width = 0;
				int height = 0;
				bool drawable = false;
				try
				{
					if (ui_)
					{
						// UIは描画ごとに入力を処理し、ポーズ中も操作とanimationを継続する。
						int contentWidth = 0, contentHeight = 0;
						window_.GetContentSize(contentWidth, contentHeight);
						window_.GetFramebufferSize(width, height);
						input_.Update();
						ui_->Update(input_.GetSnapshot(), contentWidth, contentHeight, static_cast<unsigned>((std::max)(width, 0)),
							static_cast<unsigned>((std::max)(height, 0)));
						if (uiUpdate)
						{
							uiUpdate(*ui_, elapsed);
						}
						// 最初からポーズでも、起動Sceneの完成snapshotを一度用意する。
						if (ui_->IsPaused() && !sceneHost_.GetScene())
						{
							sceneHost_.ActivateQueuedScene();
						}
					}
					for (unsigned tick = 0; tick < ticks && !stopRequested_; ++tick)
					{
						if (ui_ && ui_->IsPaused())
						{
							break;
						}
						if (simulationTicks_ == (std::numeric_limits<std::uint64_t>::max)())
						{
							throw std::overflow_error("Applicationのsimulation tick数が上限に達しました。");
						}
						if (!ui_)
						{
							input_.Update();
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
					}
					const auto* scene = sceneHost_.GetScene();
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
			// GPUの借用を解消し、完成済みCPU状態を保持して呼出側へ失敗を返す。
			if (renderer_)
			{
				renderer_->ReleaseFrame();
			}
			running_ = false;
			throw;
		}
		running_ = false;
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
			fixedClock_.Reset();
		}
	}
}
