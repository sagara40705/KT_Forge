#include <Content/MeshOptimization.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace KT::Content
{
	void MeshOptimizationSettings::Validate() const
	{
		if (mode != MeshOptimizationMode::None && mode != MeshOptimizationMode::Standard && mode != MeshOptimizationMode::StandardOverdraw)
		{
			throw std::invalid_argument("Mesh最適化の処理モードが未対応です。");
		}
		if (!std::isfinite(overdrawThreshold) || overdrawThreshold < 1)
		{
			throw std::invalid_argument("Mesh最適化のOverdraw thresholdには1以上の有限floatが必要です。");
		}
	}

	void MeshOptimizationRecord::Validate() const
	{
		settings.Validate();
		if (pipelineVersion == 0)
		{
			throw std::invalid_argument("Mesh最適化の手順versionには正のuint32が必要です。");
		}
		if (libraryRevision.size() != 40 ||
			!std::all_of(libraryRevision.begin(), libraryRevision.end(),
				[](char value) { return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'); }))
		{
			throw std::invalid_argument("Mesh最適化のライブラリ版には小文字40桁のコミットSHAが必要です。");
		}
	}

	void MeshOptimizationRecord::RequireSupported() const
	{
		Validate();
		if (settings.mode != MeshOptimizationMode::None &&
			(pipelineVersion != CurrentPipelineVersion || libraryRevision != CurrentLibraryRevision))
		{
			throw std::invalid_argument("Mesh最適化の保存済み処理版が未対応です。再取込で最適化設定を明示して移行してください。");
		}
	}
}
