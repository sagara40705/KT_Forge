#include "Application.h"
#include <Application/StartupScene.h>
#include <Core/Log.h>
#include <Graphics/Textures/ColorTargetView.h>
#include <RuntimeIntegration/SceneCamera.h>
#include <BuiltinAssets/StaticMeshAssets.h>
#include <chrono>
#include <exception>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>

namespace KT::Application
{
	Application::Application(int width, int height, const char* title, AssetSetup assetSetup, std::optional<ProjectStartup> project)
		: window_(width, height, title),
		  input_(window_),
		  graphicsDevice_(),
		  commandQueue_(graphicsDevice_),
		  sceneUpdater_(assets_)
	{
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
			sceneHost_.QueueScene(std::move(loaded.scene));
		}
		else
		{
			// 起動例も同じLoaderと製品描画経路に通し、特別な描画コードを持たない。
			sceneHost_.QueueScene(CreateStartupScene());
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

	void Application::Run(GameUpdate gameUpdate)
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
			{ gameUpdate(scene, cpu, input_.GetSnapshot(), deltaSeconds); };
		}

		running_ = true;
		stopRequested_ = false;
		timer_.Reset();
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

				// 入力を一度確定し、Scene切替・予約反映・CPU更新を直接呼ぶ。
				input_.Update();
				const auto deltaSeconds = timer_.Tick();
				sceneUpdater_.Reset();
				int width = 0;
				int height = 0;
				bool drawable = false;
				try
				{
					const auto& cpu = sceneHost_.Update(deltaSeconds, update);
					window_.GetFramebufferSize(width, height);
					drawable = width > 0 && height > 0 && !window_.IsMinimized();
					// 最小化中もCPU候補を更新する。描画する回だけCameraとviewportを評価する。
					const auto camera =
						drawable ? KT::RuntimeIntegration::SelectSceneCamera(*sceneHost_.GetScene(), renderCamera_) : std::nullopt;
					const KT::World::Viewport viewport = drawable ?
						KT::World::Viewport{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)} :
						KT::World::Viewport{};
					(void)sceneUpdater_.Update(cpu, camera, viewport);
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

				// 可変deltaで実時間を渡し、描画がない間の空回りを約60Hzに抑える。
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
		const auto backBufferIndex = swapchain_->GetCurrentBackBufferIndex();
		renderer_->Submit(frame.candidates, frame.view, KT::Graphics::ColorTargetView(*swapchain_, backBufferIndex));
		swapchain_->Present();
		renderer_->Wait();
		presentation_ = {
			presentation_.frames + 1, width, height, renderer_->LastDrawCount(), renderer_->MeshCount(), renderer_->MaterialCount()};
		if (resized)
		{
			// 初回GPU生成やResizeの待ち時間を次のゲームdeltaへ持ち越さない。
			timer_.Reset();
		}
	}
}
