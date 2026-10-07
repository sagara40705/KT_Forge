#include "Application.h"

namespace KT::Application
{
	Application::Application(int width, int height, const char* title)
		: window_(width, height, title)
	{
	}


	void Application::Run()
	{
		while (!window_.ShouldClose())
		{
			// 今ははイベントが届くまで待つ
			glfwContext_.WaitEvents();
		}
	}
}