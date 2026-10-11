#pragma once
#include <Core/Math/AABB.h>
#include <Core/Math/Vector2.h>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace KT::Content
{
	// Indexは全頂点配列への絶対参照。全Submeshで重複せずIndex列を覆う。
	struct MeshSubmesh
	{
		std::uint32_t firstIndex = 0;
		std::uint32_t indexCount = 0;
		// 配列位置ではなく名前でMaterialを対応させ、再取込時の順序変更に耐える。
		std::string materialSlot = "default";
	};

	// Smoothは既存の頂点共有を平滑化し、硬い稜線は取込で分割する。
	enum class MeshNormalMode
	{
		Preserve,
		Flat,
		Smooth
	};

	// OBJ再取込を再現する設定。sourceは素材ファイルからの相対UTF-8パス。
	struct MeshImportRecipe
	{
		std::string source;
		std::uint32_t importerVersion = 1;
		float scale = 1;
		bool flipZ = false;
		bool flipV = true;
		MeshNormalMode normals = MeshNormalMode::Preserve;
		bool generateTangents = true;
	};

	// 検証済みの不変Triangle List。空の属性配列は未指定を表し、ゼロ埋めと区別する。
	class MeshAsset
	{
	public:
		MeshAsset(std::vector<KT::Core::Math::Vector3> positions, std::vector<std::uint32_t> indices,
			std::vector<KT::Core::Math::Vector2> texcoords = {}, std::vector<KT::Core::Math::Vector3> normals = {},
			std::vector<std::array<float, 4>> tangents = {}, std::vector<MeshSubmesh> submeshes = {},
			std::optional<MeshImportRecipe> recipe = {});
		MeshAsset(const MeshAsset&) = default;
		MeshAsset(MeshAsset&&) noexcept = default;
		// 確保失敗時は既存属性・範囲・派生境界をまとめて保持する。
		MeshAsset& operator=(MeshAsset other) noexcept;
		void Validate() const;
		[[nodiscard]] std::span<const KT::Core::Math::Vector3> Positions() const noexcept;
		[[nodiscard]] std::span<const std::uint32_t> Indices() const noexcept;

		[[nodiscard]] std::span<const KT::Core::Math::Vector2> Texcoords() const noexcept
		{
			return texcoords_;
		}

		[[nodiscard]] std::span<const KT::Core::Math::Vector3> Normals() const noexcept
		{
			return normals_;
		}

		// xyzは単位接線、wはcross(normal,tangent)*wで従接線を復元する符号。
		[[nodiscard]] std::span<const std::array<float, 4>> Tangents() const noexcept
		{
			return tangents_;
		}

		[[nodiscard]] std::span<const MeshSubmesh> Submeshes() const noexcept
		{
			return submeshes_;
		}

		[[nodiscard]] std::span<const KT::Core::Math::AABB> SubmeshBounds() const noexcept
		{
			return submeshBounds_;
		}

		[[nodiscard]] const std::optional<MeshImportRecipe>& ImportRecipe() const noexcept
		{
			return recipe_;
		}

		[[nodiscard]] const KT::Core::Math::AABB& LocalBounds() const noexcept;

	private:
		std::vector<KT::Core::Math::Vector3> positions_;
		std::vector<std::uint32_t> indices_;
		std::vector<KT::Core::Math::Vector2> texcoords_;
		std::vector<KT::Core::Math::Vector3> normals_;
		std::vector<std::array<float, 4>> tangents_;
		std::vector<MeshSubmesh> submeshes_;
		std::optional<MeshImportRecipe> recipe_;
		// 参照Indexから計算する派生値。入力JSONには保存しない。
		KT::Core::Math::AABB localBounds_;
		std::vector<KT::Core::Math::AABB> submeshBounds_;
	};
}
