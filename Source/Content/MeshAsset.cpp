#include <Content/MeshAsset.h>
#include <limits>
#include <stdexcept>
#include <utility>

namespace KT::Content
{
	MeshAsset::MeshAsset(std::vector<KT::Core::Math::Vector3> positions, std::vector<std::uint32_t> indices)
		: positions_(std::move(positions)),
		  indices_(std::move(indices))
	{
		Validate();
		for (const auto index : indices_)
		{
			localBounds_ = KT::Core::Math::Expanded(localBounds_, positions_[index]);
		}
	}

	MeshAsset& MeshAsset::operator=(MeshAsset other) noexcept
	{
		positions_.swap(other.positions_);
		indices_.swap(other.indices_);
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
		if (positions_.size() > limit / sizeof(KT::Core::Math::Vector3) || indices_.size() > limit / sizeof(std::uint32_t))
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
		for (const auto index : indices_)
		{
			if (index >= positions_.size())
			{
				throw std::invalid_argument("MeshAssetのIndexが頂点配列の範囲を超えています。");
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
