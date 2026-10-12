#pragma once
#include <Application/Application.h>

namespace KT::Sandbox
{
	// Sandboxが選ぶHUD/ポーズ画面と操作解釈。UI実体はApplicationが所有する。
	class SandboxUi
	{
	public:
		explicit SandboxUi(KT::Application::Application& application);
		void Update();
		[[nodiscard]] KT::RuntimeIntegration::UiDocument Hud() const noexcept;

	private:
		// 借用先はこの画面制御より長く保持する。
		KT::Application::Application& application_;
		KT::RuntimeIntegration::GameUi& ui_;
		KT::RuntimeIntegration::UiDocument hud_, pause_;
		bool pauseVisible_ = false;
	};
}
