#include <GLFW/glfw3.h>

#include <Core/Log.h>


int main() 
{
	// GLFWの初期化
	int result = glfwInit();
	if (result == GLFW_TRUE)
	{
		(void)KT::Core::Log(KT::Core::LogLevel::Info, "GLFWの初期化に成功しました");
	}
	else
	{
		(void)KT::Core::Log(KT::Core::LogLevel::Error, "GLFWの初期化に失敗しました");
		return -1;
	}

	// OpenGLの描画機能を作らない設定
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// ウィンドウの作成
	GLFWwindow* window = glfwCreateWindow(1280, 720, "KT_Forge", nullptr, nullptr);
	if (!window)
	{
		(void)KT::Core::Log(KT::Core::LogLevel::Error, "GLFWのウィンドウの作成に失敗しました");
		glfwTerminate();
		return -1;
	}

	// 練習
	// Windowの破棄
	glfwDestroyWindow(window);

	// GLFWの終了
	glfwTerminate();

	return 0;
}