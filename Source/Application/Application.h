#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Platform/GlfwContext.h>
#include <Platform/Window.h>
#include <Graphics/GraphicsDevice.h>
#include <Graphics/Commands/CommandQueue.h>
#include <Graphics/Commands/CommandContext.h>
#include <Graphics/Presentation/Swapchain.h>
#include <memory>

namespace KT::Application
{
	// アプリケーションの本体
	class Application : private KT::Core::NonCopyable
	{
	public:
		// コンストラクタ(幅・高さ・タイトル)
		Application(int width, int height, const char* title);

		// アプリケーションの実行
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

		// Swapchain
		std::unique_ptr<KT::Graphics::Swapchain> swapchain_{};
	};
}