#include "Application.h"
#include <Core/Log.h>
#include <Graphics/FrameResources.h>
#include <Graphics/ColorTargetView.h>
#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Graph/GraphImportedTexture.h>
#include <Renderer/Passes/ClearPass.h>
#include <Renderer/Graph/GraphView.h>
#include <exception>
#include <stdexcept>

namespace KT::Application
{
	Application::Application(int width, int height, const char* title)
		: window_(width, height, title), 
		graphicsDevice_(), 
		commandQueue_(graphicsDevice_),
		commandContext_(graphicsDevice_)
	{
	}


	// 旧APIの一度だけClearする経路。TODO: 新Graphのview登録とFrameResourcesへ接続する。
	void Application::Run()
	{
		// 空のCommandListを実行してGPU完了待ちするテスト
		commandContext_.Begin();
		commandContext_.End();
		commandQueue_.ExecuteAndWait(commandContext_);
		KT_LOG_INFO("空のCommandListの実行とGPU完了待ちに成功");

		// 前検査：有効なframebuffer寸法が得られるまで待つ。閉じた場合は作成を中止する。
		int width = 0;
		int height = 0;
		window_.GetFramebufferSize(width, height);
		while (width <= 0 || height <= 0)
		{
			if (window_.ShouldClose()) 
			{
				KT_LOG_INFO("ウィンドウが閉じられたため、Swapchainの作成を中止");
				return;
			}

			// ウィンドウのサイズが0以下の場合は、イベントが届くまで待つ
			glfwContext_.WaitEvents();
			window_.GetFramebufferSize(width, height);
		}
		if (window_.ShouldClose())
		{
			KT_LOG_INFO("ウィンドウが閉じられたため、Swapchainの作成を中止");
			return;
		}
		// Swapchainの作成
		swapchain_ = std::make_unique<KT::Graphics::Swapchain>(
			graphicsDevice_, commandQueue_, window_.GetNativeHandle(),
			static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
		KT_LOG_INFO("Swapchainの作成に成功");

		const auto backBufferIndex = swapchain_->GetCurrentBackBufferIndex();

        // FrameResourcesを用意
        KT::Graphics::FrameResources frame(graphicsDevice_, commandQueue_);

		// TODO: 手動Graph ID・desc.rtv・画像handle Clearを、新しい画像/view分離APIへ移行する。
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
        KT::Renderer::AddClearPass(graph, colorHandle, { 0.0f, 0.0f, 1.0f, 1.0f });
        graph.Compile();

		// TODO: 新Recordは記録中のFrameResourcesを受け取る。以下は旧呼出。
        frame.Begin();
        graph.Record(frame);
        frame.EndRecording();
        frame.Submit();
        swapchain_->Present();
        frame.Wait();

		// 送信・表示・完了待機：画像とRTVを保持したままFence完了まで待つ。
		try
		{
			commandQueue_.Execute(commandContext_);
			swapchain_->Present();
			const auto fenceValue = commandQueue_.Signal();
			commandQueue_.Wait(fenceValue);
		}
		catch (const std::exception& error)
		{
			KT_LOG_ERROR("フレーム送信または完了待ちに失敗");
			KT_LOG_ERROR("例外: " + std::string(error.what()));
			std::terminate();
		}

		while (!window_.ShouldClose())
		{
			// イベント待機：このループでは追加の描画を行わない。
			glfwContext_.WaitEvents();
		}
	}
}
