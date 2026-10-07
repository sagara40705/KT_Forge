#include "Window.h"
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace KT::Platform
{
	Window::Window(int width, int height, const char* title)
	{
		if (width <= 0 || height <= 0 || title == nullptr)
		{
			throw std::invalid_argument("Windowの幅・高さ・タイトルが不正です。");
		}

		// GLFW_NO_APIに設定
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		// ウィンドウの作成
		window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
		if (!window_)
		{
			throw std::runtime_error("GLFWのウィンドウの作成に失敗しました。");
		}
	}

	Window::~Window()
	{
		if (window_)
		{
			glfwDestroyWindow(window_);
			window_ = nullptr;
		}
	}

	bool Window::ShouldClose() const
	{
		return glfwWindowShouldClose(window_) == GLFW_TRUE;
	}

	void Window::GetFramebufferSize(int& width, int& height) const
	{
		glfwGetFramebufferSize(window_, &width, &height);
	}
}