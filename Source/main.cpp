#include <Core/Log.h>
#include <Application/Application.h>
#include <Content/ObjMeshImporter.h>
#include <RuntimeIntegration/ModelImport.h>
#include <RuntimeIntegration/ModelPlacement.h>
#include <Content/AssetJson.h>
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
		if (command == L"--place-model" && argc == 6)
		{
			// 保存したModelを読み、既存Sceneへ別個体を追加する。外部モデルは再取込しない。
			const auto project = KT::Content::ProjectAssets::Scan(argv[2]);
			const auto model = KT::Content::AssetJson::Read(project.Resolve(argv[3]));
			if (model.Kind() != KT::Content::AssetKind::Model)
			{
				throw std::invalid_argument("--place-modelの素材ファイルにはkt.Modelが必要です。");
			}
			KT::Content::AssetCatalog catalog;
			const KT::Content::ModelAssetId id(model.id);
			catalog.RegisterBatch(project.LoadModel(id));
			catalog.Freeze();
			KT::World::ComponentRegistry components;
			KT::World::RegisterEngineComponents(components);
			components.Freeze();
			KT::World::ScriptRegistry scripts;
			scripts.Freeze();
			auto definition = KT::World::SceneJson::Read(project.Resolve(argv[4]), components, scripts);
			const auto placed = KT::RuntimeIntegration::ModelPlacement::Append(
				definition, catalog, id, components, scripts, {model.name, {}, {}});
			const auto scene = KT::World::SceneLoader::Load(definition, components, scripts);
			KT::World::SceneJson::Save(argv[5], *scene, components, scripts);
			KT_LOG_INFO("ModelをSceneへ配置しました: Model=" + id.ToString() + " / Root=" + placed.root.ToString());
			return 0;
		}
		if ((command == L"--import-obj" || command == L"--import-model") && argc >= 4)
		{
			KT::Content::ObjMeshImportOptions options;
			options.flipZ = command == L"--import-model";
			for (int index = 4; index < argc; ++index)
			{
				const std::wstring_view option = argv[index];
				if (option == L"--flip-z")
				{
					options.flipZ = true;
				}
				else if (option == L"--keep-z" && command == L"--import-model")
				{
					options.flipZ = false;
				}
				else if (option == L"--keep-v")
				{
					options.flipV = false;
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
						throw std::invalid_argument("モデルの--scaleをfloatの数値として解釈できません。");
					}
					if (consumed != value.size())
					{
						throw std::invalid_argument("モデルの--scaleには数値だけを指定してください。");
					}
				}
				else if (option == L"--no-tangents")
				{
					options.generateTangents = false;
				}
				else if (option == L"--normals" && index + 1 < argc)
				{
					const std::wstring_view value = argv[++index];
					if (value == L"preserve")
					{
						options.normals = KT::Content::MeshNormalMode::Preserve;
					}
					else if (value == L"flat")
					{
						options.normals = KT::Content::MeshNormalMode::Flat;
					}
					else if (value == L"smooth")
					{
						options.normals = KT::Content::MeshNormalMode::Smooth;
					}
					else
					{
						throw std::invalid_argument("モデルの--normalsにはpreserve/flat/smoothを指定してください。");
					}
				}
				else
				{
					throw std::invalid_argument(
						"モデル取込は--flip-z、--keep-v、--scale 数値、--normals、--no-tangents、外部モデル専用の--keep-zに対応しています。");
				}
			}
			if (command == L"--import-model")
			{
				const auto result = KT::RuntimeIntegration::ModelImport::Import(
					argv[2], argv[3], {options.scale, options.flipZ, options.flipV, options.normals, options.generateTangents});
				KT_LOG_INFO("モデルSceneを保存しました: 素材=" + std::to_string(result.assetCount) +
					" / Object=" + std::to_string(result.objectCount) + " / Model=" + result.modelId->ToString());
				for (const auto& diagnostic : result.diagnostics)
				{
					KT_LOG_WARNING(diagnostic);
				}
				return 0;
			}
			const auto result = KT::Content::ObjMeshImporter::Import(argv[2], argv[3], options);
			KT_LOG_INFO("Mesh素材を保存しました: " + result.id.ToString());
			if (result.omittedTexcoords || result.omittedNormals || result.omittedMaterials || result.mergedGroups)
			{
				KT_LOG_WARNING("Meshは法線・接線・UV0・Material slotを保存します。MTL内容とo/gの階層は保存しません。");
			}
			return 0;
		}
		if (command == L"--reimport-model" && argc == 4)
		{
			const auto result = KT::RuntimeIntegration::ModelImport::Reimport(argv[2], argv[3]);
			KT_LOG_INFO("モデルSceneを再取込しました: 素材=" + std::to_string(result.assetCount) +
				" / Object=" + std::to_string(result.objectCount) + " / Model=" + result.modelId->ToString());
			for (const auto& diagnostic : result.diagnostics)
			{
				KT_LOG_WARNING(diagnostic);
			}
			return 0;
		}
		if (command == L"--reimport-mesh" && argc == 3)
		{
			const auto result = KT::Content::ObjMeshImporter::Reimport(argv[2]);
			KT_LOG_INFO("保存済みの設定でMeshを再取込しました: " + result.id.ToString());
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
				else if (diagnostic.code == KT::RuntimeIntegration::RenderDiagnosticCode::MissingTexcoords)
				{
					const auto& id = loaded.scene->GetWorld().GetComponent<KT::World::PersistentId>(diagnostic.source);
					KT_LOG_WARNING("Texture付きMaterialのMeshにUVがありません: Object=" + id.value.ToString());
				}
				else if (diagnostic.code == KT::RuntimeIntegration::RenderDiagnosticCode::UnknownMaterialSlot)
				{
					const auto& id = loaded.scene->GetWorld().GetComponent<KT::World::PersistentId>(diagnostic.source);
					KT_LOG_WARNING(
						"Meshに割当先のMaterial slotがありません: Object=" + id.value.ToString() + " / slot=" + diagnostic.materialSlot);
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
			throw std::invalid_argument(
				"起動引数: --project root scene [--camera UUID] / --validate-project root scene / "
				"--import-obj input.obj output.asset.json [--flip-z] [--keep-v] [--scale 数値] "
				"[--normals preserve/flat/smooth] [--no-tangents] / --reimport-mesh output.asset.json / "
				"--import-model input output-directory [--keep-z] [--keep-v] [--scale 数値] "
				"[--normals preserve/flat/smooth] [--no-tangents] / --reimport-model previous-directory new-directory / "
				"--place-model root model.asset.json source.scene.json output.scene.json");
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
