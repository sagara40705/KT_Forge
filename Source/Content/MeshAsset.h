#pragma once
#include <Core/Math/AABB.h>
#include <cstdint>
#include <Core/Math/Vector2.h>
#include <span>
#include <vector>

namespace KT::Content
{
	// 検証済みの静的Triangle List。生成後は値を編集せず、Catalogで共有する。
	class MeshAsset
	{
	public:
		// 全入力の検査後にLocal AABBを計算する。退化した三角形も許可する。
		MeshAsset(std::vector<KT::Core::Math::Vector3> positions, std::vector<std::uint32_t> indices,
			std::vector<KT::Core::Math::Vector2> texcoords = {});
		MeshAsset(const MeshAsset&) = default;
		MeshAsset(MeshAsset&&) noexcept = default;
		// コピー確保が失敗しても、既存配列と派生境界の対応を保つ。
		MeshAsset& operator=(MeshAsset other) noexcept;
		// 移動済みの原本などを登録・保存しないよう、所有配列を再検査する。
		void Validate() const;
		[[nodiscard]] std::span<const KT::Core::Math::Vector3> Positions() const noexcept;
		[[nodiscard]] std::span<const std::uint32_t> Indices() const noexcept;

		[[nodiscard]] std::span<const KT::Core::Math::Vector2> Texcoords() const noexcept
		{
			return texcoords_;
		}

		[[nodiscard]] const KT::Core::Math::AABB& LocalBounds() const noexcept;

	private:
		std::vector<KT::Core::Math::Vector3> positions_;
		std::vector<std::uint32_t> indices_;
		// 未指定は空。指定時は各頂点に一つのUVを持つ。
		std::vector<KT::Core::Math::Vector2> texcoords_;
		// Indexから参照される頂点だけを包含する派生値。
		KT::Core::Math::AABB localBounds_;
	};
}
