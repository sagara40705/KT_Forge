#include <Core/Log.h>
#include <Application/Application.h>
#include <Content/ObjMeshImporter.h>
#include <BuiltinAssets/StaticMeshAssets.h>

#include <stdexcept>
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
		if (command == L"--import-obj" && argc >= 4)
		{
			KT::Content::ObjMeshImportOptions options;
			for (int index = 4; index < argc; ++index)
			{
				const std::wstring_view option = argv[index];
				if (option == L"--flip-z")
				{
					options.flipZ = true;
				}
				else if (option == L"--scale" && index + 1 < argc)
				{
					const std::wstring value = argv[++index];
					std::size_t consumed = 0;
					try
					{
						options.scale = std::stof(value, &consumed);
					}
					catch (const std::logic_error&)
					{
						throw std::invalid_argument("OBJの--scaleをfloatの数値として解釈できません。");
					}
					if (consumed != value.size())
					{
						throw std::invalid_argument("OBJの--scaleには数値だけを指定してください。");
					}
				}
				else
				{
					throw std::invalid_argument("OBJ取り込みの引数は--flip-zと--scale 数値だけに対応しています。");
				}
			}
			const auto result = KT::Content::ObjMeshImporter::Import(argv[2], argv[3], options);
			KT_LOG_INFO("Mesh素材を保存しました: " + result.id.ToString());
			if (result.omittedTexcoords || result.omittedNormals || result.omittedMaterials || result.mergedGroups)
			{
				KT_LOG_WARNING("初版Meshは位置と三角形だけを保存します。UV・法線・素材設定は保存せず、groupは一つにまとめます。");
			}
			return 0;
		}
		if (command == L"--validate-project" && argc == 4)
		{
			const auto project = KT::Content::ProjectAssets::Scan(argv[2]);
			KT::Content::AssetCatalog catalog;
			KT::BuiltinAssets::RegisterStaticMeshAssets(catalog);
			auto loaded = KT::RuntimeIntegration::ProjectSceneLoader::Load(project, argv[3], catalog);
			catalog.Freeze();
			KT::RuntimeIntegration::SceneUpdater updater(catalog);
			updater.Update(loaded.scene->GetSnapshot());
			ReportAssetDiagnostics(loaded.diagnostics);
			for (const auto& diagnostic : updater.Get().diagnostics)
			{
				if (diagnostic.code == KT::RuntimeIntegration::RenderDiagnosticCode::InvalidWorldBounds)
				{
					const auto& id = loaded.scene->GetWorld().GetComponent<KT::World::PersistentId>(diagnostic.source);
					KT_LOG_WARNING("World AABBを有限範囲で計算できません: Object=" + id.value.ToString());
				}
			}
			KT_LOG_INFO("CPUプロジェクト読込完了: 索引=" + std::to_string(project.Entries().size()) + " / 読込素材=" +
				std::to_string(loaded.loadedAssetCount) + " / 描画候補=" + std::to_string(updater.Get().candidates.objects.size()));
			return loaded.diagnostics.empty() && updater.Get().diagnostics.empty() ? 0 : 2;
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
			throw std::invalid_argument("起動引数: --project root scene [--camera UUID] / --validate-project root scene / "
										"--import-obj input.obj output.asset.json [--flip-z] [--scale 数値]");
		}
		KT::Application::Application app(1280, 720, "KT_Forge", {}, std::move(project));
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
