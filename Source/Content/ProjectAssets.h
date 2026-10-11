#pragma once
#include <Content/AssetDocument.h>
#include <filesystem>
#include <span>
#include <unordered_map>

namespace KT::Content
{
	// UUIDを正本とし、プロジェクト内での配置と種類だけを索引に保持する。
	struct ProjectAssetEntry
	{
		AssetId id;
		AssetKind kind;
		std::string name;
		std::filesystem::path relativePath;
	};

	// 起動時の素材索引。Scan/Load中の外部ファイル変更は行わず、変更後は再Scanする。
	// *.asset.jsonを全件検証するが、素材本体は保持しない。Scene/Worldには依存しない。
	class ProjectAssets
	{
	public:
		ProjectAssets(const ProjectAssets&) = delete;
		ProjectAssets& operator=(const ProjectAssets&) = delete;
		ProjectAssets(ProjectAssets&&) noexcept = default;
		ProjectAssets& operator=(ProjectAssets&&) noexcept = default;
		static constexpr std::size_t MaxFiles = 100000;
		static constexpr std::uintmax_t MaxScanBytes = 512 * 1024 * 1024;
		[[nodiscard]] static ProjectAssets Scan(const std::filesystem::path& root);
		[[nodiscard]] const std::filesystem::path& Root() const noexcept;
		[[nodiscard]] std::span<const ProjectAssetEntry> Entries() const noexcept;
		// 借用はこの索引の変更・移動・破棄まで有効。
		[[nodiscard]] const ProjectAssetEntry* Find(AssetId id) const;
		// 存在する参照を一度だけ読み込む。欠落IDは呼出側がFindで診断する。
		// 読込失敗・索引からのID/種類変更は例外とし、部分的な結果を公開しない。
		[[nodiscard]] std::vector<AssetDocument> Load(std::span<const AssetId> ids) const;
		// 相対配置を解決する。絶対パス、親への脱出、リンク経由のroot外参照を拒否する。
		[[nodiscard]] std::filesystem::path Resolve(const std::filesystem::path& relativePath) const;

	private:
		ProjectAssets() = default;
		std::filesystem::path root_;
		// 相対パス順を固定し、ディレクトリ列挙順に結果を依存させない。
		std::vector<ProjectAssetEntry> entries_;
		std::unordered_map<AssetId, std::size_t, AssetIdHash> byId_;
	};
}
