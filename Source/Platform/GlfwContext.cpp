#include "GlfwContext.h"
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace
{
	// GlfwContextが既に存在するか
	bool contextActive = false;
}

namespace KT::Platform
{
	GlfwContext::GlfwContext()
	{
		if (contextActive)
		{
			throw std::logic_error("GlfwContextは既に存在します。");
		}
		if (glfwInit() != GLFW_TRUE)
		{
			throw std::runtime_error("GLFWの初期化に失敗しました。");
		}
		contextActive = true;
	}

	GlfwContext::~GlfwContext()
	{
		glfwTerminate();
		contextActive = false;
	}

	void GlfwContext::PollEvents()
	{
		glfwPollEvents();
	}

	void GlfwContext::WaitEvents()
	{
		glfwWaitEvents();
	}
}