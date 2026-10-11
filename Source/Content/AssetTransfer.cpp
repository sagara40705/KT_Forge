#include <Content/AssetTransfer.h>
#include <Content/AssetJson.h>
#include <fstream>
#include <set>

namespace KT::Content
{
	AssetDocument AssetTransfer::CloneMaterial(const ProjectAssets& project, MaterialAssetId source, std::string name)
	{
		const auto* entry = project.Find(source.Value());
		if (!entry || entry->kind != AssetKind::Material)
		{
			throw std::invalid_argument("複製するMaterialが欠落または種類違いです: " + source.ToString());
		}
		const std::array ids{source.Value()};
		auto document = project.Load(ids).front();
		const auto texture = std::get<MaterialAsset>(document.data).Settings().baseColorTexture;
		if (texture)
		{
			const auto* dependency = project.Find(texture->Value());
			if (!dependency || dependency->kind != AssetKind::Texture)
			{
				throw std::invalid_argument("複製元MaterialのTextureが欠落または種類違いです。");
			}
			const std::array textures{texture->Value()};
			(void)project.Load(textures);
		}
		do
		{
			document.id = AssetId::Generate();
		} while (project.Find(document.id));
		document.name = std::move(name);
		(void)AssetJson::Stringify(document);
		return document;
	}

	AssetTransferResult AssetTransfer::Collect(
		const ProjectAssets& source, std::span<const AssetDocument> preferred, std::span<const AssetId> required)
	{
		// 新しい原本を優先し、旧Materialが参照する同一IDのTextureも新定義へ接続する。
		std::map<AssetId, const AssetDocument*> replacements;
		for (const auto& document : preferred)
		{
			if (!replacements.emplace(document.id, &document).second)
			{
				throw std::invalid_argument("引継ぎ先の素材UUIDが重複しています。");
			}
		}
		std::vector<std::pair<AssetId, std::optional<AssetKind>>> pending;
		for (const auto id : required)
		{
			pending.emplace_back(id, std::nullopt);
		}
		std::set<AssetId> visited;
		AssetTransferResult result;
		std::size_t imageBytes = 0;
		for (std::size_t index = 0; index < pending.size(); ++index)
		{
			const auto [id, expected] = pending[index];
			const auto replacement = replacements.find(id);
			const auto* entry = source.Find(id);
			if (replacement == replacements.end() && !entry)
			{
				throw std::invalid_argument("引き継ぐ個体専用資源が旧projectにありません: " + id.ToString());
			}
			const auto kind = replacement != replacements.end() ? replacement->second->Kind() : entry->kind;
			if (expected && *expected != kind)
			{
				throw std::invalid_argument("引き継ぐ資源の依存種類が一致しません: " + id.ToString());
			}
			if (!visited.insert(id).second)
			{
				continue;
			}
			if (visited.size() > ProjectAssets::MaxFiles)
			{
				throw std::invalid_argument("引継ぎ資源数が10万件を超えています。");
			}
			auto document = replacement != replacements.end() ? *replacement->second : AssetJson::Read(source.Resolve(entry->relativePath));
			if (document.id != id || document.Kind() != kind)
			{
				throw std::invalid_argument("索引作成後に引継ぎ資源のUUIDまたは種類が変わりました。");
			}
			if (const auto* model = std::get_if<ModelAsset>(&document.data))
			{
				for (const auto& node : model->Nodes())
				{
					if (node.mesh)
					{
						pending.emplace_back(node.mesh->Value(), AssetKind::Mesh);
					}
					if (node.material)
					{
						pending.emplace_back(node.material->Value(), AssetKind::Material);
					}
					for (const auto& [slot, material] : node.materials)
					{
						if (material)
						{
							pending.emplace_back(material->Value(), AssetKind::Material);
						}
					}
				}
			}
			if (const auto* material = std::get_if<MaterialAsset>(&document.data); material && material->Settings().baseColorTexture)
			{
				pending.emplace_back(material->Settings().baseColorTexture->Value(), AssetKind::Texture);
			}
			if (replacement != replacements.end())
			{
				continue;
			}
			// 画像配置をUUID別に付け替え、新bundleの取込画像や同名画像を上書きしない。
			if (auto* texture = std::get_if<TextureAsset>(&document.data))
			{
				const auto text = texture->Source();
				const auto image = source.Resolve(std::filesystem::path(std::u8string(text.begin(), text.end())));
				std::ifstream input(image, std::ios::binary | std::ios::ate);
				const std::streamoff size = input.tellg();
				if (!input || size <= 0 || size > static_cast<std::streamoff>(TextureAsset::MaxDecodedBytes))
				{
					throw std::invalid_argument("引継ぎTextureの画像を開けないか、サイズが256MiBを超えています。");
				}
				if (static_cast<std::size_t>(size) > ProjectAssets::MaxScanBytes - imageBytes)
				{
					throw std::invalid_argument("引継ぎ画像の合計が512MiBを超えています。");
				}
				imageBytes += static_cast<std::size_t>(size);
				std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
				input.seekg(0);
				if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)))
				{
					throw std::runtime_error("引継ぎTextureの画像を最後まで読み込めません。");
				}
				const auto relative = std::filesystem::path("retained-images") / id.ToString() / image.filename();
				const auto utf8 = relative.generic_u8string();
				*texture = TextureAsset(std::string(utf8.begin(), utf8.end()), texture->ColorSpace(), texture->MipMode());
				result.images.emplace(relative, std::move(bytes));
			}
			result.assets.push_back(std::move(document));
		}
		return result;
	}
}
