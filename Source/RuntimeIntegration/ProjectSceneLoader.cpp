#include <RuntimeIntegration/ProjectSceneLoader.h>
#include <array>
#include <stdexcept>

namespace KT::RuntimeIntegration
{
	ProjectSceneLoadResult ProjectSceneLoader::Load(const KT::Content::ProjectAssets& project, const std::filesystem::path& scenePath,
		KT::Content::AssetCatalog& catalog, const KT::World::ComponentRegistry& components, const KT::World::ScriptRegistry& scripts)
	{
		if (catalog.IsFrozen())
		{
			throw std::logic_error("ProjectSceneLoaderには起動登録中のAssetCatalogが必要です。");
		}

		// 組込み素材も同じUUID空間を使い、未使用ファイルの衝突も先に拒否する。
		for (const auto& entry : project.Entries())
		{
			if (catalog.FindKind(entry.id))
			{
				throw std::invalid_argument("プロジェクト素材と登録済み素材のUUIDが重複しています: " + entry.id.ToString());
			}
		}
		const auto asset = KT::World::SceneJson::Read(project.Resolve(scenePath), components, scripts);
		ProjectSceneLoadResult result;
		std::vector<KT::Content::AssetId> required;
		for (const auto& object : asset.objects)
		{
			for (const auto& component : object.components)
			{
				if (component.type != "kt.MeshRenderer")
				{
					continue;
				}
				// 現在の素材間依存はない。Texture等を追加するときは参照収集も拡張する。
				const std::array fields{
					std::pair{"mesh", KT::Content::AssetKind::Mesh}, std::pair{"material", KT::Content::AssetKind::Material}};
				for (const auto& [field, expected] : fields)
				{
					const auto& value = component.data.At(field);
					if (std::holds_alternative<std::nullptr_t>(value.value))
					{
						continue;
					}
					const auto id = KT::Content::AssetId::Parse(value.AsString());
					auto actual = catalog.FindKind(id);
					if (const auto* entry = project.Find(id))
					{
						actual = entry->kind;
						required.push_back(id);
					}
					if (!actual || *actual != expected)
					{
						result.diagnostics.push_back({object.uuid, component.type, field, id, expected, actual});
					}
				}
			}
		}

		// Script実体を含めScene生成を終えてから素材を公開し、例外時はCatalogを変更しない。
		auto documents = project.Load(required);
		result.loadedAssetCount = documents.size();
		result.scene = KT::World::SceneLoader::Load(asset, components, scripts);
		catalog.RegisterBatch(std::move(documents));
		return result;
	}

	ProjectSceneLoadResult ProjectSceneLoader::Load(
		const KT::Content::ProjectAssets& project, const std::filesystem::path& scenePath, KT::Content::AssetCatalog& catalog)
	{
		KT::World::ComponentRegistry components;
		KT::World::RegisterEngineComponents(components);
		components.Freeze();
		KT::World::ScriptRegistry scripts;
		scripts.Freeze();
		return Load(project, scenePath, catalog, components, scripts);
	}
}
