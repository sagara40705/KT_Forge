#include <Application/ModelWorkflowCommands.h>
#include <RuntimeIntegration/ModelWorkflowJson.h>
#include <RuntimeIntegration/ModelReconcileJson.h>
#include <Content/AssetTransfer.h>
#include <Content/AssetJson.h>
#include <World/Scene/SceneJson.h>
#include <Core/Log.h>
#include <algorithm>

namespace KT::Application
{
	std::optional<int> RunModelWorkflowCommand(int argc, wchar_t* argv[])
	{
		using namespace KT::RuntimeIntegration;
		const std::wstring_view command = argc > 1 ? argv[1] : L"";
		if (command == L"--inspect-model-reimport")
		{
			if (argc != 4)
			{
				throw std::invalid_argument("--inspect-model-reimportには旧bundleと出力JSONを指定してください。");
			}
			ModelWorkflowJson::SaveNodeTable(argv[3], ModelImport::InspectReimport(argv[2]));
			KT_LOG_INFO("再取込前後のkeyと旧NodeIDを対応表JSONへ保存しました。");
			return 0;
		}
		if (command == L"--clone-material")
		{
			if (argc != 5)
			{
				throw std::invalid_argument("--clone-materialにはproject root、元Material JSON、出力JSONを指定してください。");
			}
			const auto project = KT::Content::ProjectAssets::Scan(argv[2]);
			const auto original = KT::Content::AssetJson::Read(project.Resolve(argv[3]));
			if (original.Kind() != KT::Content::AssetKind::Material || std::filesystem::exists(argv[4]))
			{
				throw std::invalid_argument("複製元にはMaterial、保存先には未使用のファイル名が必要です。");
			}
			const auto cloned =
				KT::Content::AssetTransfer::CloneMaterial(project, KT::Content::MaterialAssetId(original.id), original.name + " Instance");
			KT::Content::AssetJson::Save(argv[4], cloned);
			KT_LOG_INFO("個体用Materialを複製しました。割当先へこのIDを指定してください: " + cloned.id.ToString());
			return 0;
		}
		if (command == L"--reimport-model")
		{
			if (argc < 4)
			{
				throw std::invalid_argument("--reimport-modelには旧bundleと新規bundleを指定してください。");
			}
			std::optional<std::filesystem::path> mapping;
			std::optional<std::filesystem::path> retainedRoot;
			std::optional<std::filesystem::path> retainedScene;
			bool acceptChanges = false;
			std::optional<KT::Content::MeshOptimizationSettings> optimization;
			for (int index = 4; index < argc; ++index)
			{
				const std::wstring_view option = argv[index];
				if (option == L"--node-map" && index + 1 < argc && !mapping)
				{
					mapping = argv[++index];
				}
				else if (option == L"--carry-scene" && index + 2 < argc && !retainedRoot)
				{
					retainedRoot = argv[++index];
					retainedScene = argv[++index];
				}
				else if (option == L"--accept-node-changes" && !acceptChanges)
				{
					acceptChanges = true;
				}
				else if (option == L"--mesh-optimization" && index + 1 < argc && !optimization)
				{
					const std::wstring_view mode = argv[++index];
					KT::Content::MeshOptimizationSettings settings;
					if (mode == L"none")
					{
						settings.mode = KT::Content::MeshOptimizationMode::None;
					}
					else if (mode == L"standard")
					{
						settings.mode = KT::Content::MeshOptimizationMode::Standard;
					}
					else if (mode == L"overdraw")
					{
						settings.mode = KT::Content::MeshOptimizationMode::StandardOverdraw;
					}
					else
					{
						throw std::invalid_argument("Model再取込の--mesh-optimizationにはnone/standard/overdrawを指定してください。");
					}
					optimization = settings;
				}
				else
				{
					throw std::invalid_argument("Model再取込の指定が不足・重複・未対応です。--node-map、--carry-scene、--accept-node-"
												"changes、--mesh-optimizationに対応しています。");
				}
			}
			auto options = mapping ? ModelWorkflowJson::ReadCorrespondence(*mapping) : ModelReimportOptions{};
			options.optimizationOverride = optimization;
			options.acceptUnmatchedNodes = options.acceptUnmatchedNodes || acceptChanges;
			if (retainedRoot)
			{
				const auto project = KT::Content::ProjectAssets::Scan(*retainedRoot);
				KT::World::ComponentRegistry components;
				KT::World::RegisterEngineComponents(components);
				components.Freeze();
				KT::World::ScriptRegistry scripts;
				scripts.Freeze();
				const auto path = retainedScene->is_absolute() ? *retainedScene : project.Resolve(*retainedScene);
				const auto scene = KT::World::SceneJson::Read(path, components, scripts);
				options.retainedProject = *retainedRoot;
				options.retainedAssets = CollectModelSceneAssets(scene, components);
			}
			const auto result = ModelImport::Reimport(argv[2], argv[3], options);
			KT_LOG_INFO("モデルを再取込しました: 素材=" + std::to_string(result.assetCount) + " / Model=" + result.modelId->ToString());
			for (const auto& diagnostic : result.diagnostics)
			{
				KT_LOG_WARNING(diagnostic);
			}
			return 0;
		}
		if (command != L"--reconcile-models")
		{
			return std::nullopt;
		}
		if (argc < 5)
		{
			throw std::invalid_argument("--reconcile-modelsには新project root、元Scene、出力Sceneを指定してください。");
		}
		std::optional<std::filesystem::path> resolutionPath;
		std::optional<std::filesystem::path> selectionPath;
		for (int index = 5; index < argc; ++index)
		{
			const std::wstring_view option = argv[index];
			if (option == L"--resolutions" && index + 1 < argc && !resolutionPath)
			{
				resolutionPath = argv[++index];
			}
			else if (option == L"--selection" && index + 1 < argc && !selectionPath)
			{
				selectionPath = argv[++index];
			}
			else if (index == 5 && !option.starts_with(L"--") && !resolutionPath)
			{
				// 従来の末尾解決JSON指定を互換維持する。
				resolutionPath = argv[index];
			}
			else
			{
				throw std::invalid_argument("Model反映の指定が不足・重複・未対応です。--selectionと--resolutionsを使用してください。");
			}
		}
		// 原本は新snapshotから読み、元Sceneには明示した旧snapshotの絶対パスも許可する。
		const auto project = KT::Content::ProjectAssets::Scan(argv[2]);
		std::vector<KT::Content::AssetId> ids;
		for (const auto& entry : project.Entries())
		{
			ids.push_back(entry.id);
		}
		KT::Content::AssetCatalog catalog;
		catalog.RegisterBatch(project.Load(ids));
		catalog.Freeze();
		KT::World::ComponentRegistry components;
		KT::World::RegisterEngineComponents(components);
		components.Freeze();
		KT::World::ScriptRegistry scripts;
		scripts.Freeze();
		const std::filesystem::path source(argv[3]);
		auto definition = KT::World::SceneJson::Read(source.is_absolute() ? source : project.Resolve(source), components, scripts);
		const bool linked = std::any_of(definition.objects.begin(), definition.objects.end(),
			[](const auto& object)
			{
				return std::any_of(object.components.begin(), object.components.end(),
					[](const auto& component) { return component.type == "kt.ModelInstance"; });
			});
		if (!linked)
		{
			throw std::invalid_argument("SceneにModelInstanceリンクがありません。旧配置には明示対応でAttachしてください。");
		}
		const auto resolutions = resolutionPath ? ReadModelResolutions(*resolutionPath) : ModelReconcileOptions{};
		const auto selection = selectionPath ? ModelWorkflowJson::ReadSelection(*selectionPath) : ModelReconcileSelection{};
		const auto plan = ModelReconciler::Plan(definition, catalog, components, scripts, resolutions, selection);
		for (const auto& conflict : plan.Conflicts())
		{
			KT_LOG_WARNING("Model反映競合: Root=" + conflict.instance.ToString() +
				" / Node=" + (conflict.node ? conflict.node->ToString() : "Scene") + " / " + conflict.reason);
		}
		ModelReconciler::Apply(definition, plan, components, scripts);
		KT::World::SceneJson::Save(argv[4], definition, components, scripts);
		KT_LOG_INFO("Model再取込結果を指定対象のScene配置へ反映しました。");
		return 0;
	}
}
