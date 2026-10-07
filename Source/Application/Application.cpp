#include "Application.h"
#include <Core/Log.h>

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

		while (!window_.ShouldClose())
		{
			// 今ははイベントが届くまで待つ
			glfwContext_.WaitEvents();
		}
	}
}