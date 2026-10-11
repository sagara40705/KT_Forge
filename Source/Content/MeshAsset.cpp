#include <Content/MeshAsset.h>
#include <limits>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <filesystem>

namespace KT::Content
{
	MeshAsset::MeshAsset(std::vector<KT::Core::Math::Vector3> positions, std::vector<std::uint32_t> indices,
		std::vector<KT::Core::Math::Vector2> texcoords, std::vector<KT::Core::Math::Vector3> normals,
		std::vector<std::array<float, 4>> tangents, std::vector<MeshSubmesh> submeshes, std::optional<MeshImportRecipe> recipe)
		: positions_(std::move(positions)),
		  indices_(std::move(indices)),
		  texcoords_(std::move(texcoords)),
		  normals_(std::move(normals)),
		  tangents_(std::move(tangents)),
		  submeshes_(std::move(submeshes)),
		  recipe_(std::move(recipe))
	{
		// 旧単一MeshはIndex全体とdefault slotへ明示変換する。
		if (submeshes_.empty() && !indices_.empty() && indices_.size() <= (std::numeric_limits<std::uint32_t>::max)())
		{
			submeshes_.push_back({0, static_cast<std::uint32_t>(indices_.size()), "default"});
		}
		Validate();
		for (const auto& submesh : submeshes_)
		{
			KT::Core::Math::AABB bounds;
			for (std::size_t index = submesh.firstIndex; index < std::size_t(submesh.firstIndex) + submesh.indexCount; ++index)
			{
				bounds = KT::Core::Math::Expanded(bounds, positions_[indices_[index]]);
				localBounds_ = KT::Core::Math::Expanded(localBounds_, positions_[indices_[index]]);
			}
			submeshBounds_.push_back(bounds);
		}
	}

	MeshAsset& MeshAsset::operator=(MeshAsset other) noexcept
	{
		positions_.swap(other.positions_);
		indices_.swap(other.indices_);
		texcoords_.swap(other.texcoords_);
		normals_.swap(other.normals_);
		tangents_.swap(other.tangents_);
		submeshes_.swap(other.submeshes_);
		recipe_.swap(other.recipe_);
		submeshBounds_.swap(other.submeshBounds_);
		std::swap(localBounds_, other.localBounds_);
		return *this;
	}

	void MeshAsset::Validate() const
	{
		if (positions_.empty() || indices_.empty())
		{
			throw std::invalid_argument("MeshAssetの頂点とIndexは空にできません。");
		}
		// CPUの要素数とbyte数を、初版の32bit範囲で表現できることを確認する。
		constexpr auto limit = (std::numeric_limits<std::uint32_t>::max)();
		if (positions_.size() > limit / 48 || indices_.size() > limit / sizeof(std::uint32_t))
		{
			throw std::invalid_argument("MeshAssetの頂点またはIndexのbyte数がuint32範囲を超えています。");
		}
		if (indices_.size() % 3 != 0)
		{
			throw std::invalid_argument("MeshAssetのIndex数はTriangle Listのため3の倍数が必要です。");
		}
		for (const auto position : positions_)
		{
			if (!KT::Core::Math::IsFinite(position))
			{
				throw std::invalid_argument("MeshAssetの頂点座標に非有限値があります。");
			}
		}
		if (!texcoords_.empty() && texcoords_.size() != positions_.size())
		{
			throw std::invalid_argument("MeshAssetのUV数は頂点数との一致が必要です。");
		}
		for (const auto uv : texcoords_)
		{
			if (!std::isfinite(uv.x) || !std::isfinite(uv.y))
			{
				throw std::invalid_argument("MeshAssetのUVには有限値が必要です。");
			}
		}
		for (const auto index : indices_)
		{
			if (index >= positions_.size())
			{
				throw std::invalid_argument("MeshAssetのIndexが頂点配列の範囲を超えています。");
			}
		}
		// 属性は未指定か全頂点分。法線と接線の無効な既定値を保存しない。
		if ((!normals_.empty() && normals_.size() != positions_.size()) ||
			(!tangents_.empty() && (tangents_.size() != positions_.size() || normals_.empty() || texcoords_.empty())))
		{
			throw std::invalid_argument("Meshの法線・接線の数、または接線に必要な法線・UV0が不正です。");
		}
		for (std::size_t index = 0; index < normals_.size(); ++index)
		{
			const auto normal = normals_[index];
			const double length = double(normal.x) * normal.x + double(normal.y) * normal.y + double(normal.z) * normal.z;
			if (!KT::Core::Math::IsFinite(normal) || std::abs(length - 1) > 0.002)
			{
				throw std::invalid_argument("Meshの法線には有限の単位Vector3が必要です。");
			}
			if (!tangents_.empty())
			{
				const auto& tangent = tangents_[index];
				const double tangentLength =
					double(tangent[0]) * tangent[0] + double(tangent[1]) * tangent[1] + double(tangent[2]) * tangent[2];
				const double dot = double(normal.x) * tangent[0] + double(normal.y) * tangent[1] + double(normal.z) * tangent[2];
				if (!std::isfinite(tangentLength) || std::abs(tangentLength - 1) > 0.002 || !std::isfinite(dot) || std::abs(dot) > 0.002 ||
					(tangent[3] != 1 && tangent[3] != -1))
				{
					throw std::invalid_argument("Meshの接線には法線と直交する単位XYZと符号w=1/-1が必要です。");
				}
			}
		}
		if (!tangents_.empty())
		{
			for (std::size_t first = 0; first < indices_.size(); first += 3)
			{
				const auto sign = tangents_[indices_[first]][3];
				if (tangents_[indices_[first + 1]][3] != sign || tangents_[indices_[first + 2]][3] != sign)
				{
					throw std::invalid_argument("一つの三角形の接線w符号は一致が必要です。");
				}
			}
		}
		std::size_t nextIndex = 0;
		for (const auto& submesh : submeshes_)
		{
			if (submesh.firstIndex != nextIndex || submesh.indexCount == 0 || submesh.indexCount % 3 != 0 || nextIndex > indices_.size() ||
				submesh.indexCount > indices_.size() - nextIndex)
			{
				throw std::invalid_argument("SubmeshはIndex列を順に重複なく覆う、空でない三角形範囲が必要です。");
			}
			if (submesh.materialSlot.empty() || submesh.materialSlot.size() > 256 || submesh.materialSlot.find('\0') != std::string::npos)
			{
				throw std::invalid_argument("SubmeshのMaterial slotにはNULを含まない1..256byteの名前が必要です。");
			}
			nextIndex += submesh.indexCount;
		}
		if (nextIndex != indices_.size())
		{
			throw std::invalid_argument("Submeshに属さないIndexがあります。");
		}
		if (recipe_)
		{
			const auto& recipe = *recipe_;
			const std::filesystem::path path(std::u8string(recipe.source.begin(), recipe.source.end()));
			if (recipe.source.empty() || recipe.source.find('\0') != std::string::npos || path.is_absolute() || path.has_root_name() ||
				recipe.importerVersion != 1 || !std::isfinite(recipe.scale) || recipe.scale <= 0 ||
				(recipe.normals != MeshNormalMode::Preserve && recipe.normals != MeshNormalMode::Flat &&
					recipe.normals != MeshNormalMode::Smooth))
			{
				throw std::invalid_argument("MeshのOBJ取込recipeの相対パス・version・Scale・法線生成方針が不正です。");
			}
		}
	}

	std::span<const KT::Core::Math::Vector3> MeshAsset::Positions() const noexcept
	{
		return positions_;
	}

	std::span<const std::uint32_t> MeshAsset::Indices() const noexcept
	{
		return indices_;
	}

	const KT::Core::Math::AABB& MeshAsset::LocalBounds() const noexcept
	{
		return localBounds_;
	}
}
