#include "Window.h"
#include "Input/Input.h"
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <stdexcept>
#include <exception>

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

		glfwSetWindowUserPointer(window_, this);
		glfwSetKeyCallback(window_, &Window::HandleKey);
		glfwSetMouseButtonCallback(window_, &Window::HandleMouseButton);
		glfwSetCursorPosCallback(window_, &Window::HandleCursorPosition);
		glfwSetScrollCallback(window_, &Window::HandleScroll);
		glfwSetWindowFocusCallback(window_, &Window::HandleFocus);
	}

	Window::~Window()
	{
		// 借用中のWindow破棄を見逃し、Inputにdangling参照を残さない。
		if (input_ != nullptr)
		{
			std::terminate();
		}
		if (window_)
		{
			glfwSetWindowUserPointer(window_, nullptr);
			glfwDestroyWindow(window_);
			window_ = nullptr;
		}
	}

	void Window::AttachInput(Input& input)
	{
		if (input_ != nullptr)
		{
			throw std::logic_error("Windowには既にInputが接続されています。");
		}
		input_ = &input;
	}

	void Window::DetachInput(Input& input) noexcept
	{
		if (input_ == &input)
		{
			input_ = nullptr;
		}
	}

	void Window::HandleKey(GLFWwindow* window, int key, int, int action, int) noexcept
	{
		auto* owner = static_cast<Window*>(glfwGetWindowUserPointer(window));
		if (owner != nullptr && owner->input_ != nullptr)
		{
			owner->input_->OnKey(key, action);
		}
	}

	void Window::HandleMouseButton(GLFWwindow* window, int button, int action, int) noexcept
	{
		auto* owner = static_cast<Window*>(glfwGetWindowUserPointer(window));
		if (owner != nullptr && owner->input_ != nullptr)
		{
			owner->input_->OnMouseButton(button, action);
		}
	}

	void Window::HandleCursorPosition(GLFWwindow* window, double x, double y) noexcept
	{
		auto* owner = static_cast<Window*>(glfwGetWindowUserPointer(window));
		if (owner != nullptr && owner->input_ != nullptr)
		{
			owner->input_->OnCursorPosition(x, y);
		}
	}

	void Window::HandleScroll(GLFWwindow* window, double x, double y) noexcept
	{
		auto* owner = static_cast<Window*>(glfwGetWindowUserPointer(window));
		if (owner != nullptr && owner->input_ != nullptr)
		{
			owner->input_->OnScroll(x, y);
		}
	}

	void Window::HandleFocus(GLFWwindow* window, int focused) noexcept
	{
		auto* owner = static_cast<Window*>(glfwGetWindowUserPointer(window));
		if (owner != nullptr && owner->input_ != nullptr)
		{
			owner->input_->OnFocus(focused == GLFW_TRUE);
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

	HWND Window::GetNativeHandle() const
	{
		HWND hwnd = glfwGetWin32Window(window_);
		if (!hwnd)
		{
			throw std::runtime_error("GLFWのネイティブハンドルの取得に失敗しました。");
		}
		return hwnd;
	}
}