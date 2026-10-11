#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace KT::Content
{
	// 処理の依存順を固定し、Overdrawだけを選択する。
	enum class MeshOptimizationMode
	{
		None,
		Standard,
		StandardOverdraw
	};

	// 新規取込では属性値を保持する標準最適化を使用する。
	struct MeshOptimizationSettings
	{
		MeshOptimizationMode mode = MeshOptimizationMode::Standard;
		// Overdrawによる頂点キャッシュ効率の悪化許容。性能改善率ではない。
		float overdrawThreshold = 1.05f;

		void Validate() const;
		bool operator==(const MeshOptimizationSettings&) const = default;
	};

	// 再取込の再現に必要な設定と処理実装の版。保存済み幾何の読込では版を要求しない。
	struct MeshOptimizationRecord
	{
		static constexpr std::uint32_t CurrentPipelineVersion = 1;
		static constexpr std::string_view CurrentLibraryRevision = "9e1f07b159d3cb777f1c67ed31fc11fd117986f4";

		MeshOptimizationSettings settings;
		std::uint32_t pipelineVersion = CurrentPipelineVersion;
		std::string libraryRevision{CurrentLibraryRevision};

		void Validate() const;
		// 通常再取込で未知の処理版を黙って置き換えない。
		void RequireSupported() const;
	};

	// 要素数だけを記録する。GPU時間やキャッシュ改善率の実測値ではない。
	struct MeshOptimizationStatistics
	{
		std::size_t inputVertexCount = 0;
		std::size_t outputVertexCount = 0;
		std::size_t indexCount = 0;
		std::size_t submeshCount = 0;
		MeshOptimizationMode mode = MeshOptimizationMode::None;
	};
}
