#include "Application.h"
#include <Core/Log.h>
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


	void Application::Run()
	{
		// 空のCommandListを実行してGPU完了待ちするテスト
		commandContext_.Begin();
		commandContext_.End();
		commandQueue_.ExecuteAndWait(commandContext_);
		KT_LOG_INFO("空のCommandListの実行とGPU完了待ちに成功");

		// ウィンドウのサイズを取得
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

		while (!window_.ShouldClose())
		{
			// 今ははイベントが届くまで待つ
			glfwContext_.WaitEvents();
		}
	}
}