#pragma once
#include <Content/AssetId.h>
#include <Core/Math/Quaternion.h>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace KT::Content
{
	class AssetCatalog;

	// 配置前のノード定義。親は先行ノードの位置で、実行時Entityを保持しない。
	struct ModelNode
	{
		// モデル内だけで一意の照合key。配置個体のObjectUuidとは独立する。
		std::string key;
		std::string name;
		// nullはモデル内のroot。複数rootも保持できる。
		std::optional<std::size_t> parent;
		// LH/+Y up、meter、Quaternion xyzw、Local=S*R*Tの入力値。
		KT::Core::Math::Vector3 position{};
		KT::Core::Math::Quaternion rotation{};
		KT::Core::Math::Vector3 scale{1, 1, 1};
		std::optional<MeshAssetId> mesh;
		// 個別設定がないslotで使う共通Material。nullは未割当。
		std::optional<MaterialAssetId> material;
		// 未指定slotは共通Materialへfallbackし、明示nullはそのslotを描画しない。
		std::map<std::string, std::optional<MaterialAssetId>> materials;
		// 名前や親の変更後も維持する。再取込のkeyとは区別する。
		ModelNodeId id{AssetId::Generate()};
	};

	// 階層と初期設定を共有する不変CPU資源。形状・画像・実行状態は参照先が所有する。
	class ModelAsset
	{
	public:
		static constexpr std::size_t MaxNodes = 100000;
		static constexpr std::size_t MaxDepth = 128;
		explicit ModelAsset(std::vector<ModelNode> nodes);
		ModelAsset(const ModelAsset&) = default;
		ModelAsset(ModelAsset&&) noexcept = default;
		// 確保に失敗した場合は元のノード定義を保持する。
		ModelAsset& operator=(ModelAsset other) noexcept;
		void Validate() const;
		// 固定済みCatalogで依存の存在・種類・slotを検査し、部分配置を防ぐ。
		void ValidateReferences(const AssetCatalog& catalog) const;
		[[nodiscard]] std::span<const ModelNode> Nodes() const noexcept;

	private:
		// 親が先行する順を保存し、循環検査と配置を一回の走査で行う。
		std::vector<ModelNode> nodes_;
	};
}
