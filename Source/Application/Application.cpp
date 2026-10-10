#include "Application.h"
#include <Core/Log.h>
#include <Graphics/Commands/FrameResources.h>
#include <Graphics/Textures/ColorTargetView.h>
#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Graph/GraphImportedTexture.h>
#include <Renderer/Passes/ClearPass.h>
#include <Renderer/Graph/GraphView.h>
#include <World/Scene/SceneLoader.h>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <thread>
#include <utility>

namespace KT::Application
{
	Application::Application(int width, int height, const char* title)
		: window_(width, height, title),
		  input_(window_),
		  graphicsDevice_(),
		  commandQueue_(graphicsDevice_)
	{
		// 起動Sceneも定義から生成し、Script更新をせずCPU検証した候補を予約する。
		KT::World::SceneAsset initialScene;
		initialScene.name = "DefaultScene";
		sceneHost_.QueueScene(KT::World::SceneLoader::Load(initialScene));
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

	void Application::RequestStop() noexcept
	{
		stopRequested_ = true;
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
			{
				gameUpdate(scene, cpu, input_.GetSnapshot(), deltaSeconds);
			};
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
				try
				{
					(void)sceneHost_.Update(deltaSeconds, update);
				}
				catch (...)
				{
					KT_LOG_ERROR("SceneのCPU更新に失敗したため、Applicationの通常更新を停止します。");
					throw;
				}

				// CPU成功後だけ初期表示を行う。画面寸法が0でもScene更新は続ける。
				if (stopRequested_ || window_.ShouldClose())
				{
					break;
				}
				int width = 0;
				int height = 0;
				window_.GetFramebufferSize(width, height);
				if (!initialClearPresented_ && width > 0 && height > 0)
				{
					PresentInitialClear(width, height);
					// 初期GPU作成と完了待ちの時間を次のゲームdeltaへ持ち越さない。
					timer_.Reset();
				}

				// 可変deltaで実時間を渡し、描画がない間の空回りを約60Hzに抑える。
				std::this_thread::sleep_until(updateStarted + std::chrono::duration<double>(1.0 / 60.0));
			}
		}
		catch (...)
		{
			// 適用済みScene状態は保持する。復旧・Scene交換をせず呼出側へ失敗を返す。
			running_ = false;
			throw;
		}
		running_ = false;
	}

	void Application::PresentInitialClear(int width, int height)
	{
		// 有効な画面寸法でSwapchainを作る。初期表示の再試行では同じ所有先を使う。
		if (!swapchain_)
		{
			swapchain_ = std::make_unique<KT::Graphics::Swapchain>(graphicsDevice_, commandQueue_, window_.GetNativeHandle(),
				static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
			KT_LOG_INFO("Swapchainの作成に成功");
		}

		const auto backBufferIndex = swapchain_->GetCurrentBackBufferIndex();

		// 送信・表示・完了待機：画像とRTVを保持したままFence完了まで待つ。
		try
		{
			KT::Graphics::FrameResources frame(graphicsDevice_, commandQueue_);
			KT::Renderer::RenderGraph graph;

			// BackBufferのImport
			KT::Renderer::GraphImportedTextureDesc output{};
			output.name = "BackBuffer";
			output.resource = swapchain_->GetBackBuffer(backBufferIndex);
			output.initialState = D3D12_RESOURCE_STATE_PRESENT;
			output.finalState = D3D12_RESOURCE_STATE_PRESENT;
			output.contentsDefined = false;
			output.requireDefineAtEnd = true;
			const auto outputHandle = graph.ImportTexture(output);

			// 色Viewを登録する
			KT::Renderer::GraphViewDesc colorView{};
			colorView.name = "BackBufferRTV";
			colorView.resource = outputHandle;
			colorView.binding = KT::Graphics::ColorTargetView(*swapchain_, backBufferIndex);
			const auto colorHandle = graph.AddView(std::move(colorView));

			// Passの追加
			KT::Renderer::AddClearPass(graph, colorHandle, {0.0f, 1.0f, 1.0f, 1.0f});
			graph.Compile();

			// 同期1slotの経路で一度だけ送信・表示し、借用画像の最終利用完了を待つ。
			frame.Begin();
			graph.Record(frame);
			frame.EndRecording();
			frame.Submit();
			swapchain_->Present();
			frame.Wait();
			initialClearPresented_ = true;
		}
		catch (const std::exception& error)
		{
			KT_LOG_ERROR("フレーム送信または完了待ちに失敗");
			KT_LOG_ERROR("例外: " + std::string(error.what()));
			throw;
		}
	}
}
