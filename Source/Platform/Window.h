#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Windows.h>

// 　前方宣言
struct GLFWwindow;

namespace KT::Platform
{
	class Input;

	// ウィンドウの所有と状態の取得
	// GlfwContextの初期化後に生成し、GlfwContextの終了前に破棄する。操作はメインスレッドで行う
	// 接続したInputを先に破棄する。借用中のWindow破棄は契約違反として終了する。
	class Window : private KT::Core::NonCopyable
	{
	public:
		// コンストラクタ(幅・高さ・タイトル)
		Window(int width, int height, const char* title);

		// デストラクタ(ウィンドウの破棄)
		~Window();

		// 閉じる要求を取得する
		bool ShouldClose() const;

		// 画面領域のピクセル寸法を取得する
		void GetFramebufferSize(int& width, int& height) const;

	private:
		friend class Input;
		// callback/user pointerはWindowが管理する。Inputは非所有で1つだけ接続する。
		void AttachInput(Input& input);
		void DetachInput(Input& input) noexcept;
		static void HandleKey(GLFWwindow* window, int key, int scancode, int action, int mods) noexcept;
		static void HandleMouseButton(GLFWwindow* window, int button, int action, int mods) noexcept;
		static void HandleCursorPosition(GLFWwindow* window, double x, double y) noexcept;
		static void HandleScroll(GLFWwindow* window, double x, double y) noexcept;
		static void HandleFocus(GLFWwindow* window, int focused) noexcept;
		Input* input_ = nullptr;

		// GLFWのウィンドウポインタ
		GLFWwindow* window_ = nullptr;

	public:
		// Windowsのネイティブハンドルを取得する
		HWND GetNativeHandle() const;
	};
}