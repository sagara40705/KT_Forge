#include <Content/ProjectAssets.h>
#include <Content/AssetJson.h>
#include <Content/AssetCatalog.h>
#include <Content/TextureLoader.h>
#include <algorithm>
#include <set>
#include <stdexcept>

namespace KT::Content
{
	static std::string PathText(const std::filesystem::path& path)
	{
		const auto text = path.generic_u8string();
		return {text.begin(), text.end()};
	}

	// 日本語ファイル名もUTF-8で診断し、解析失敗と読込失敗を区別する。
	static AssetDocument ReadAsset(const std::filesystem::path& path)
	{
		try
		{
			return AssetJson::Read(path);
		}
		catch (const std::runtime_error& error)
		{
			throw std::runtime_error("プロジェクト素材の読込に失敗しました: " + PathText(path) + " / " + error.what());
		}
		catch (const std::invalid_argument& error)
		{
			throw std::invalid_argument("プロジェクト素材の検証に失敗しました: " + PathText(path) + " / " + error.what());
		}
	}

	// canonical後の相対配置で確認し、junctionなどによるroot外への参照も拒否する。
	static std::filesystem::path RelativeInside(const std::filesystem::path& root, const std::filesystem::path& path)
	{
		const auto relative = path.lexically_relative(root);
		if (relative.empty() || relative.has_root_path())
		{
			throw std::invalid_argument("プロジェクトrootとの相対配置を解決できません。");
		}
		for (const auto& part : relative)
		{
			if (part == "..")
			{
				throw std::invalid_argument("プロジェクトroot外を参照する配置は使用できません。");
			}
		}
		return relative;
	}

	ProjectAssets ProjectAssets::Scan(const std::filesystem::path& root)
	{
		ProjectAssets result;
		result.root_ = std::filesystem::canonical(root);
		if (!std::filesystem::is_directory(result.root_))
		{
			throw std::invalid_argument("素材プロジェクトのrootはディレクトリが必要です。");
		}

		// 手動でディレクトリを巡回し、リンクの循環や同一配置の重複探索を拒否する。
		std::vector<std::filesystem::path> directories{result.root_};
		std::set<std::filesystem::path> visited{result.root_};
		std::vector<std::filesystem::path> files;
		std::uintmax_t bytes = 0;
		for (std::size_t index = 0; index < directories.size(); ++index)
		{
			for (const auto& entry : std::filesystem::directory_iterator(directories[index]))
			{
				const auto resolved = std::filesystem::canonical(entry.path());
				const auto relative = RelativeInside(result.root_, resolved);
				if (entry.is_directory())
				{
					if (directories.size() >= MaxFiles || !visited.insert(resolved).second)
					{
						throw std::invalid_argument("素材ディレクトリの重複・循環または探索上限を検出しました。");
					}
					directories.push_back(resolved);
					continue;
				}
				if (!entry.is_regular_file())
				{
					continue;
				}
				auto filename = entry.path().filename().wstring();
				for (auto& character : filename)
				{
					if (character >= L'A' && character <= L'Z')
					{
						character += L'a' - L'A';
					}
				}
				if (!filename.ends_with(L".asset.json"))
				{
					continue;
				}
				const auto size = entry.file_size();
				if (files.size() >= MaxFiles || size > MaxScanBytes - bytes)
				{
					throw std::invalid_argument("素材索引のファイル数または合計512MiBの上限を超えています。");
				}
				bytes += size;
				files.push_back(relative);
			}
		}
		std::sort(files.begin(), files.end());
		for (const auto& relative : files)
		{
			const auto document = ReadAsset(result.Resolve(relative));
			const auto kind = document.Kind();
			const auto [found, inserted] = result.byId_.emplace(document.id, result.entries_.size());
			if (!inserted)
			{
				throw std::invalid_argument("素材UUIDが重複しています: " + document.id.ToString() + " / " + PathText(relative) + " / " +
					PathText(result.entries_[found->second].relativePath));
			}
			result.entries_.push_back({document.id, kind, document.name, relative});
		}
		return result;
	}

	const std::filesystem::path& ProjectAssets::Root() const noexcept
	{
		return root_;
	}

	std::span<const ProjectAssetEntry> ProjectAssets::Entries() const noexcept
	{
		return entries_;
	}

	const ProjectAssetEntry* ProjectAssets::Find(AssetId id) const
	{
		const auto found = byId_.find(id);
		return found == byId_.end() ? nullptr : &entries_[found->second];
	}

	std::filesystem::path ProjectAssets::Resolve(const std::filesystem::path& relativePath) const
	{
		if (root_.empty() || relativePath.empty() || relativePath.has_root_path())
		{
			throw std::invalid_argument("プロジェクト内の空でない相対パスが必要です。");
		}
		for (const auto& part : relativePath)
		{
			if (part == ".." || part.native().find(L':') != std::wstring::npos)
			{
				throw std::invalid_argument("プロジェクト相対パスに親ディレクトリや代替データstreamは指定できません。");
			}
		}
		const auto resolved = std::filesystem::canonical(root_ / relativePath);
		(void)RelativeInside(root_, resolved);
		return resolved;
	}

	std::vector<AssetDocument> ProjectAssets::LoadModel(ModelAssetId id) const
	{
		// 辺の種類を照合してから読む。共有参照はUUIDで一意化する。
		std::vector<std::pair<AssetId, AssetKind>> required{{id.Value(), AssetKind::Model}};
		std::set<AssetId> loaded;
		std::vector<AssetDocument> documents;
		for (std::size_t index = 0; index < required.size(); ++index)
		{
			const auto [assetId, expected] = required[index];
			const auto* entry = Find(assetId);
			if (!entry || entry->kind != expected)
			{
				throw std::invalid_argument("Modelの依存素材が欠落または種類違いです: " + assetId.ToString());
			}
			if (!loaded.insert(assetId).second)
			{
				continue;
			}
			const std::array ids{assetId};
			auto values = Load(ids);
			auto document = std::move(values.front());
			if (const auto* model = std::get_if<ModelAsset>(&document.data))
			{
				for (const auto& node : model->Nodes())
				{
					if (node.mesh)
					{
						required.emplace_back(node.mesh->Value(), AssetKind::Mesh);
					}
					if (node.material)
					{
						required.emplace_back(node.material->Value(), AssetKind::Material);
					}
					for (const auto& [slot, material] : node.materials)
					{
						if (material)
						{
							required.emplace_back(material->Value(), AssetKind::Material);
						}
					}
				}
			}
			if (const auto* material = std::get_if<MaterialAsset>(&document.data); material && material->Settings().baseColorTexture)
			{
				required.emplace_back(material->Settings().baseColorTexture->Value(), AssetKind::Texture);
			}
			documents.push_back(std::move(document));
		}
		// 閉じた依存集合を一時Catalogで検査し、成功するまで呼出側へ公開しない。
		AssetCatalog catalog;
		catalog.RegisterBatch(documents);
		catalog.Freeze();
		catalog.Resolve(std::optional(id)).asset->ValidateReferences(catalog);
		return documents;
	}

	std::vector<AssetDocument> ProjectAssets::Load(std::span<const AssetId> ids) const
	{
		// 索引位置を一意化し、共有参照と診断順による重複読込を防ぐ。
		std::set<std::size_t> selected;
		for (const auto id : ids)
		{
			const auto found = byId_.find(id);
			if (found != byId_.end())
			{
				selected.insert(found->second);
			}
		}
		std::vector<AssetDocument> documents;
		documents.reserve(selected.size());
		for (const auto index : selected)
		{
			const auto& entry = entries_[index];
			auto document = ReadAsset(Resolve(entry.relativePath));
			const auto kind = document.Kind();
			if (document.id != entry.id || kind != entry.kind)
			{
				throw std::invalid_argument("索引作成後に素材のUUIDまたは種類が変わりました: " + PathText(entry.relativePath));
			}
			// 画像の配置もroot境界で検査し、参照されたTextureだけを展開する。
			if (auto* texture = std::get_if<TextureAsset>(&document.data))
			{
				const auto& source = texture->Source();
				const std::u8string path(source.begin(), source.end());
				*texture = TextureLoader::Load(*texture, Resolve(std::filesystem::path(path)));
			}
			documents.push_back(std::move(document));
		}
		return documents;
	}
}
