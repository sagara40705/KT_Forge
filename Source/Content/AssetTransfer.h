#pragma once
#include <Content/ProjectAssets.h>
#include <map>

namespace KT::Content
{
	// 新bundleへ追加する原本と画像。既存bundleを変更せず、呼出側が一時保存後に公開する。
	struct AssetTransferResult
	{
		std::vector<AssetDocument> assets;
		std::map<std::filesystem::path, std::vector<std::uint8_t>> images;
	};

	// 制作時の資源複製とsnapshot間の引継ぎ。WorldとGPUを生成しない。
	class AssetTransfer
	{
	public:
		// 色・設定を値コピーし、新UUIDを発行する。Texture参照は共有する。
		[[nodiscard]] static AssetDocument CloneMaterial(const ProjectAssets& project, MaterialAssetId source, std::string name);
		// preferredの同一UUIDを優先し、そこにない資源と依存だけを旧projectから引き継ぐ。
		// 欠落・種類違い・画像読込失敗では所有結果を返さず、双方のprojectを保持する。
		[[nodiscard]] static AssetTransferResult Collect(
			const ProjectAssets& source, std::span<const AssetDocument> preferred, std::span<const AssetId> required);
	};
}
