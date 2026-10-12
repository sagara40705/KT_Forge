#include <UiDemo.h>
#include <Application/Application.h>
#include <SandboxUi.h>
#include <ToonDemo.h>
#include <Core/Log.h>
#include <cmath>

namespace KT::Sandbox
{
	using KT::Application::Application;

	void RunUiDemo(unsigned tickLimit)
	{
		Application app(1280, 720, "KT_Forge - RmlUi", RegisterToonDemoAssets, {}, CreateToonDemoScene);
		auto& ui = app.UI();
		SandboxUi screens(app);
		unsigned ticks = 0;
		app.Run(
			[&](auto&, const auto&, const auto&, double)
			{
				++ticks;
				// ゲームのHPを値でコピーし、SceneやComponentをUIへ保持させない。
				const float health = 50 + 50 * std::sin(ticks / 90.0f);
				ui.SetHealth(screens.Hud(), "hp-fill", health, 100);
				ui.SetText(screens.Hud(), "hp-label", "HP " + std::to_string(static_cast<int>(health)) + " / 100");
				if (tickLimit != 0 && ticks >= tickLimit)
				{
					app.RequestStop();
				}
			},
			[&](auto&, double) { screens.Update(); });
		KT_LOG_INFO("UI描画例を終了しました。tick=" + std::to_string(app.CompletedSimulationTicks()) +
			" / Present=" + std::to_string(app.GetPresentationStatistics().frames));
	}
}
