#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Windows.h>

//　前方宣言
struct GLFWwindow;

namespace KT::Platform
{
	// ウィンドウの所有と状態の取得
	// GlfwContextの初期化後に生成し、GlfwContextの終了前に破棄する。操作はメインスレッドで行う
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
		// GLFWのウィンドウポインタ
		GLFWwindow* window_ = nullptr;

	public:
		// Windowsのネイティブハンドルを取得する
		HWND GetNativeHandle() const;
	};
}