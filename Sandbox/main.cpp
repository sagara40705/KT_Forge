#include <Core/Log.h>
#include <Application/Application.h>
#include <PhysicsDemo.h>
#include <AnimationDemo.h>
#include <ToonDemo.h>
#include <EffectDemo.h>
#include <UiDemo.h>
#include <World/Lighting/DirectionalLight.h>
#include <cmath>
#include <World/Animation/Animator.h>
#include <StartupScene.h>

#include <AssetCommands.h>

#include <stdexcept>
#include <algorithm>
#include <string>

// 参照元の永続UUIDを表示し、Sceneファイル側で欠落参照を特定できるようにする。
static void ReportAssetDiagnostics(std::span<const KT::RuntimeIntegration::SceneAssetDiagnostic> diagnostics)
{
	for (const auto& diagnostic : diagnostics)
	{
		KT_LOG_WARNING("素材参照" + std::string(diagnostic.actual ? "の種類違い: " : "の欠落: ") + diagnostic.asset.ToString() +
			" / Object=" + diagnostic.object.ToString() + " / " + diagnostic.component + "." + diagnostic.field);
	}
}

// WindowsのUnicodeパスを保持し、CPU素材操作ではWindow/Graphicsを生成しない。
int wmain(int argc, wchar_t* argv[])
{
	try
	{
		const std::wstring_view command = argc > 1 ? argv[1] : L"";
		if (command == L"--effect-demo")
		{
			if (argc != 3 && (argc != 5 || std::wstring_view(argv[3]) != L"--ticks"))
			{
				throw std::invalid_argument("Effect描画例の使用法: --effect-demo <effect.efkefc> [--ticks 1..6000]");
			}
			unsigned ticks = 0;
			if (argc == 5)
			{
				for (const auto digit : std::wstring_view(argv[4]))
				{
					if (digit < L'0' || digit > L'9' || ticks > 600)
					{
						throw std::invalid_argument("Effect描画例の終了tickは1..6000の整数です。");
					}
					ticks = ticks * 10 + static_cast<unsigned>(digit - L'0');
				}
				if (ticks < 1 || ticks > 6000)
				{
					throw std::invalid_argument("Effect描画例の終了tickは1..6000の整数です。");
				}
			}
			KT::Sandbox::RunEffectDemo(argv[2], ticks);
			return 0;
		}
		if (command == L"--physics-demo" || command == L"--animation-demo" || command == L"--toon-demo" || command == L"--ui-demo")
		{
			// 終了tickを指定すると、同じ製品の描画経路を有限回だけ実行できる。
			unsigned tickLimit = 0;
			if (argc != 2)
			{
				if (argc != 4 || std::wstring_view(argv[2]) != L"--ticks")
				{
					throw std::invalid_argument("描画例の使用法: --physics-demo/--animation-demo/--toon-demo/--ui-demo [--ticks 1..6000]");
				}
				unsigned count = 0;
				for (const auto character : std::wstring_view(argv[3]))
				{
					if (character < L'0' || character > L'9' || count > 600)
					{
						throw std::invalid_argument("描画例の終了tickは1..6000の整数で指定してください。");
					}
					count = count * 10 + static_cast<unsigned>(character - L'0');
				}
				if (count < 1 || count > 6000)
				{
					throw std::invalid_argument("描画例の終了tickは1..6000の整数で指定してください。");
				}
				tickLimit = count;
			}
			const bool toonDemo = command == L"--toon-demo";
			if (command == L"--ui-demo")
			{
				KT::Sandbox::RunUiDemo(tickLimit);
				return 0;
			}
			const bool animationDemo = command == L"--animation-demo" || toonDemo;
			KT::Application::Application app(1280, 720, "KT_Forge - Demo",
				toonDemo		  ? KT::Application::Application::AssetSetup(KT::Sandbox::RegisterToonDemoAssets) :
					animationDemo ? KT::Application::Application::AssetSetup(KT::Sandbox::RegisterAnimationDemoAssets) :
									KT::Application::Application::AssetSetup{},
				{},
				toonDemo		  ? KT::Sandbox::CreateToonDemoScene :
					animationDemo ? KT::Sandbox::CreateAnimationDemoScene :
									KT::Sandbox::CreatePhysicsDemoScene);
			unsigned ticks = 0;
			app.Run(
				[&](auto& scene, const auto&, const auto&, double)
				{
					++ticks;
					if (toonDemo)
					{
						// ゲームから光の向きと影範囲を編集し、同じtickの描画へ反映する。
						scene.template ForEachActive<KT::World::DirectionalLight>(
							[&](auto, auto& light)
							{
								light.direction = {0.4f + 0.2f * std::sin(ticks / 60.0f), -1, 0.3f};
								light.shadowCenter = {0, 0, 3};
							});
					}
					// ゲーム更新から個体の再生値を編集し、二Clipを遷移する例。
					if (animationDemo && (ticks == 120 || ticks == 240))
					{
						scene.template ForEachActive<KT::World::Animator>(
							[&](auto, auto& animator) { animator.CrossFade(ticks == 120 ? "Idle" : "Sway", 0.5); });
					}
					if (tickLimit != 0 && ticks >= tickLimit)
					{
						app.RequestStop();
					}
				});
			KT_LOG_INFO("描画例を終了しました。tick=" + std::to_string(app.CompletedSimulationTicks()) +
				" / Present=" + std::to_string(app.GetPresentationStatistics().frames));
			return 0;
		}
		if (const auto result = KT::Tools::RunAssetCommand(argc, argv))
		{
			return *result;
		}
		std::optional<KT::Application::Application::ProjectStartup> project;
		if (command == L"--project" && (argc == 4 || (argc == 6 && std::wstring_view(argv[4]) == L"--camera")))
		{
			project = KT::Application::Application::ProjectStartup{argv[2], argv[3], {}, {}};
			if (argc == 6)
			{
				// UUIDはASCIIだけを許し、Unicodeパスの変換とは分離する。
				std::string camera;
				for (const auto character : std::wstring_view(argv[5]))
				{
					if (character > 127)
					{
						throw std::invalid_argument("描画CameraのUUIDはASCII文字で指定してください。");
					}
					camera.push_back(static_cast<char>(character));
				}
				project->camera = KT::World::ObjectUuid::Parse(camera);
			}
		}
		else if (argc != 1)
		{
			throw std::invalid_argument(
				"起動引数: --project root scene [--camera UUID] / --validate-project root scene / "
				"--import-obj input.obj output.asset.json [--flip-z] [--keep-v] [--scale 数値] "
				"[--normals preserve/flat/smooth] [--no-tangents] / --reimport-mesh output.asset.json / "
				"--import-model input output-directory [--keep-z] [--keep-v] [--scale 数値] "
				"[--normals preserve/flat/smooth] [--no-tangents] / --reimport-model previous-directory new-directory "
				"[--node-map correspondence.json] [--carry-scene old-root scene.json] [--accept-node-changes] / "
				"--inspect-model-reimport previous-directory correspondence.json / --clone-material root material.asset.json "
				"new.asset.json / "
				"--place-model root model.asset.json source.scene.json output.scene.json / "
				"--reconcile-models new-root source.scene.json output.scene.json [--selection selection.json] [--resolutions "
				"resolutions.json]");
		}
		KT::Application::Application app(1280, 720, "KT_Forge - Sandbox", {}, project,
			project ? KT::Application::Application::SceneFactory{} : KT::Sandbox::CreateStartupScene);
		ReportAssetDiagnostics(app.GetAssetDiagnostics());
		app.Run();
	}
	catch (const std::exception& error)
	{
		KT_LOG_ERROR(error.what());
		return 1;
	}

	return 0;
}
