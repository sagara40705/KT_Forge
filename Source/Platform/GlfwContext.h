#pragma once
#include <Core/Utility/NonCopyable.h>

namespace KT::Platform
{
	// GLFW全体を使える期間の管理とイベント処理
	// 生成・イベント処理・破棄はメインスレッドで行う
	class GlfwContext : private KT::Core::NonCopyable
	{
	public:
		// コンストラクタ(glfwInitを呼ぶ)
		GlfwContext();

		// デストラクタ(glfwTerminateを呼ぶ)
		~GlfwContext();

		// 届いているイベントを処理
		void PollEvents();

		// イベントが届くまで待つ
		void WaitEvents();
	};
}