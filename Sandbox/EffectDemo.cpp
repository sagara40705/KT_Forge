#include <EffectDemo.h>
#include <Application/Application.h>
#include <ToonDemo.h>
#include <Core/Log.h>
#include <cmath>

namespace KT::Sandbox
{
	using KT::Application::Application;

	void RunEffectDemo(const std::filesystem::path& path, unsigned tickLimit)
	{
		Application app(1280, 720, "KT_Forge - Effekseer", RegisterToonDemoAssets, {}, CreateToonDemoScene);
		auto& effects = app.Effects();
		const auto asset = effects.Load(path);
		KT::RuntimeIntegration::EffectHandle attached;
		KT::RuntimeIntegration::EffectHandle fixed;
		unsigned ticks = 0;
		app.Run(
			[&](auto& scene, const auto& cpu, const auto&, double)
			{
				++ticks;
				// 再生途中のhandleを使い回さず、終了後は次の攻撃として生成する。
				if (ticks % 120 == 1)
				{
					for (const auto& input : cpu.Inputs())
					{
						if (input.mesh)
						{
							attached = effects.PlayAttached(asset, input.entity, KT::Core::Math::Scale({0.5f, 0.5f, 0.5f}));
							break;
						}
					}
					fixed = effects.Play(asset, KT::Core::Math::TRS({0.5f, 0.5f, 0.5f}, {}, {4, 1, 3}));
				}
				if (ticks % 120 == 90)
				{
					effects.Stop(fixed);
					effects.Stop(attached);
				}
				// EntityのWorld変換への追従は完成CPU結果から反映される。
				scene.template ForEachActive<KT::World::MeshRenderer, KT::World::LocalTransform>(
					[&](auto, auto&, auto& transform)
					{
						if (transform.scale.y > 1)
						{
							transform.position.y = 0.3f * std::sin(ticks / 30.0f);
						}
					});
				if (tickLimit != 0 && ticks >= tickLimit)
				{
					app.RequestStop();
				}
			});
		KT_LOG_INFO("Effect描画例を終了しました。tick=" + std::to_string(app.CompletedSimulationTicks()) +
			" / Present=" + std::to_string(app.GetPresentationStatistics().frames));
	}
}
