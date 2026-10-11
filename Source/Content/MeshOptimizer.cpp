#include <Content/MeshOptimizer.h>
#include <meshoptimizer.h>
#include <array>
#include <algorithm>
#include <type_traits>
#include <utility>

namespace KT::Content
{
	static_assert(MESHOPTIMIZER_VERSION == 1030);
	static_assert(std::is_same_v<std::uint32_t, unsigned int>);
	static_assert(sizeof(KT::Core::Math::Vector3) == 12 && sizeof(KT::Core::Math::Vector2) == 8);

	// 全属性へ一つの対応表を適用する。空属性は未指定のまま保持する。
	template <class T> static std::vector<T> Remap(std::span<const T> source, std::span<const unsigned int> remap, std::size_t vertexCount)
	{
		if (source.empty())
		{
			return {};
		}
		std::vector<T> result(vertexCount);
		meshopt_remapVertexBuffer(result.data(), source.data(), source.size(), sizeof(T), remap.data());
		return result;
	}

	MeshOptimizationResult MeshOptimizer::Optimize(const MeshAsset& source, MeshOptimizationSettings settings)
	{
		// 確保やライブラリ呼出より前に入力契約を検査する。
		source.Validate();
		settings.Validate();
		MeshOptimizationStatistics statistics{
			source.Positions().size(), source.Positions().size(), source.Indices().size(), source.Submeshes().size(), settings.mode};
		if (settings.mode == MeshOptimizationMode::None)
		{
			return {source, statistics};
		}

		// 属性のbyte列を完全一致で比較し、UV継ぎ目・硬い法線・接線符号を残す。
		std::array<meshopt_Stream, 4> streams{};
		std::size_t streamCount = 0;
		streams[streamCount++] = {source.Positions().data(), 12, sizeof(KT::Core::Math::Vector3)};
		if (!source.Texcoords().empty())
		{
			streams[streamCount++] = {source.Texcoords().data(), 8, sizeof(KT::Core::Math::Vector2)};
		}
		if (!source.Normals().empty())
		{
			streams[streamCount++] = {source.Normals().data(), 12, sizeof(KT::Core::Math::Vector3)};
		}
		if (!source.Tangents().empty())
		{
			streams[streamCount++] = {source.Tangents().data(), 16, sizeof(std::array<float, 4>)};
		}
		std::vector<unsigned int> remap(source.Positions().size());
		const auto uniqueCount = meshopt_generateVertexRemapMulti(
			remap.data(), source.Indices().data(), source.Indices().size(), source.Positions().size(), streams.data(), streamCount);
		std::vector<std::uint32_t> indices(source.Indices().size());
		meshopt_remapIndexBuffer(indices.data(), source.Indices().data(), indices.size(), remap.data());
		auto positions = Remap(source.Positions(), remap, uniqueCount);
		auto texcoords = Remap(source.Texcoords(), remap, uniqueCount);
		auto normals = Remap(source.Normals(), remap, uniqueCount);
		auto tangents = Remap(source.Tangents(), remap, uniqueCount);
		std::vector<MeshSubmesh> submeshes(source.Submeshes().begin(), source.Submeshes().end());
		// Overdrawのfloat積・差のoverflowを避ける。比較用座標だけを一様に正規化する。
		std::vector<KT::Core::Math::Vector3> overdrawPositions;
		if (settings.mode == MeshOptimizationMode::StandardOverdraw)
		{
			const auto& bounds = source.LocalBounds();
			const double extent = (std::max)({double(bounds.maximum.x) - bounds.minimum.x, double(bounds.maximum.y) - bounds.minimum.y,
				double(bounds.maximum.z) - bounds.minimum.z});
			const double inverseExtent = extent == 0 ? 0 : 1 / extent;
			overdrawPositions.reserve(positions.size());
			for (const auto position : positions)
			{
				overdrawPositions.push_back({static_cast<float>((double(position.x) - bounds.minimum.x) * inverseExtent),
					static_cast<float>((double(position.y) - bounds.minimum.y) * inverseExtent),
					static_cast<float>((double(position.z) - bounds.minimum.z) * inverseExtent)});
			}
		}

		// Draw範囲の内部だけを並べ替え、Material所属を変更しない。
		for (const auto& submesh : submeshes)
		{
			auto* range = indices.data() + submesh.firstIndex;
			meshopt_optimizeVertexCache(range, range, submesh.indexCount, uniqueCount);
			if (settings.mode == MeshOptimizationMode::StandardOverdraw)
			{
				meshopt_optimizeOverdraw(range, range, submesh.indexCount, &overdrawPositions.front().x, uniqueCount,
					sizeof(KT::Core::Math::Vector3), settings.overdrawThreshold);
			}
		}

		// 最終Index列全体から配置を決め、全属性とIndexへ同じ対応表を適用する。
		remap.resize(uniqueCount);
		const auto fetchedCount = meshopt_optimizeVertexFetchRemap(remap.data(), indices.data(), indices.size(), uniqueCount);
		meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());
		positions = Remap<KT::Core::Math::Vector3>(positions, remap, fetchedCount);
		texcoords = Remap<KT::Core::Math::Vector2>(texcoords, remap, fetchedCount);
		normals = Remap<KT::Core::Math::Vector3>(normals, remap, fetchedCount);
		tangents = Remap<std::array<float, 4>>(tangents, remap, fetchedCount);

		// 新しいMeshの検証と境界計算が成功した結果だけを返す。
		statistics.outputVertexCount = fetchedCount;
		return {MeshAsset(std::move(positions), std::move(indices), std::move(texcoords), std::move(normals), std::move(tangents),
					std::move(submeshes), source.ImportRecipe()),
			statistics};
	}
}
