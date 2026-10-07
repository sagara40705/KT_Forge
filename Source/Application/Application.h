#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Platform/GlfwContext.h>
#include <Platform/Window.h>
#include <Graphics/GraphicsDevice.h>
#include <Graphics/CommandQueue.h>
#include <Graphics/CommandContext.h>

namespace KT::Application
{
	// アプリケーションの本体
	class Application : private KT::Core::NonCopyable
	{
	public:
		// コンストラクタ(幅・高さ・タイトル)
		Application(int width, int height, const char* title);

		// メインループを開始する
		void Run();

	private:
		// GlfwContextの所有
		KT::Platform::GlfwContext glfwContext_{};

		// ウィンドウの所有
		KT::Platform::Window window_;

		// GraphicsDevice
		KT::Graphics::GraphicsDevice graphicsDevice_{};

		// CommandQueue
		KT::Graphics::CommandQueue commandQueue_;

		// CommandContext
		KT::Graphics::CommandContext commandContext_;
	};
}