#pragma once
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace KT::Content
{
	// 配置や実行時handleから独立した素材の永続UUID。全ゼロは無効値。
	struct AssetId
	{
		std::array<std::uint8_t, 16> bytes{};
		[[nodiscard]] bool IsValid() const noexcept;
		[[nodiscard]] static AssetId Generate();
		[[nodiscard]] static AssetId Parse(std::string_view text);
		[[nodiscard]] std::string ToString() const;
		auto operator<=>(const AssetId&) const = default;
	};

	// Catalog内の種類をまたいで重複UUIDを検査する。
	struct AssetIdHash
	{
		std::size_t operator()(const AssetId& id) const noexcept;
	};

	// 素材の参照を型で分ける。未割当はoptionalで表し、無効IDを保持しない。
	template <class Tag> class TypedAssetId
	{
	public:
		explicit TypedAssetId(AssetId id)
			: id_(id)
		{
			if (!id.IsValid())
			{
				throw std::invalid_argument("素材参照に全ゼロのAssetIdは指定できません。");
			}
		}

		[[nodiscard]] static TypedAssetId Parse(std::string_view text)
		{
			return TypedAssetId(AssetId::Parse(text));
		}

		[[nodiscard]] AssetId Value() const noexcept
		{
			return id_;
		}

		[[nodiscard]] std::string ToString() const
		{
			return id_.ToString();
		}

		auto operator<=>(const TypedAssetId&) const = default;

	private:
		AssetId id_;
	};

	struct MeshAssetTag;
	struct MaterialAssetTag;
	struct TextureAssetTag;
	struct ModelAssetTag;
	using MeshAssetId = TypedAssetId<MeshAssetTag>;
	using MaterialAssetId = TypedAssetId<MaterialAssetTag>;
	using TextureAssetId = TypedAssetId<TextureAssetTag>;
	using ModelAssetId = TypedAssetId<ModelAssetTag>;
}
