#include <RuntimeIntegration/ProjectSceneLoader.h>
#include <array>
#include <unordered_map>
#include <unordered_set>
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
		std::vector<std::pair<KT::World::ObjectUuid, KT::Content::MaterialAssetId>> materials;
		for (const auto& object : asset.objects)
		{
			for (const auto& component : object.components)
			{
				if (component.type != "kt.MeshRenderer")
				{
					continue;
				}
				// 非Activeも含め、Sceneが保持する永続参照から必要素材を収集する。
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
					if (expected == KT::Content::AssetKind::Material)
					{
						materials.emplace_back(object.uuid, KT::Content::MaterialAssetId(id));
					}
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
		// Material依存を一段展開する。Textureには他素材への依存を許可しない。
		std::unordered_map<KT::Content::AssetId, std::size_t, KT::Content::AssetIdHash> pending;
		for (std::size_t index = 0; index < documents.size(); ++index)
		{
			pending.emplace(documents[index].id, index);
		}
		std::vector<KT::Content::AssetId> textures;
		for (const auto& [object, materialId] : materials)
		{
			auto textureId = catalog.MaterialTexture(materialId);
			if (const auto found = pending.find(materialId.Value()); found != pending.end())
			{
				if (const auto* material = std::get_if<KT::Content::MaterialAsset>(&documents[found->second].data))
				{
					textureId = material->Settings().baseColorTexture;
				}
			}
			if (!textureId)
			{
				continue;
			}
			const auto id = textureId->Value();
			auto actual = catalog.FindKind(id);
			if (const auto* entry = project.Find(id))
			{
				actual = entry->kind;
				if (entry->kind == KT::Content::AssetKind::Texture && !pending.contains(id))
				{
					textures.push_back(id);
				}
			}
			if (!actual || *actual != KT::Content::AssetKind::Texture)
			{
				result.diagnostics.push_back(
					{object, "kt.MeshRenderer", "material.baseColorTexture", id, KT::Content::AssetKind::Texture, actual});
			}
		}
		for (auto& texture : project.Load(textures))
		{
			documents.push_back(std::move(texture));
		}
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
