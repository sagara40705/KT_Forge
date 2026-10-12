#pragma once
#include <Animation/SkeletonAnimation.h>
#include <RuntimeIntegration/RenderFrame.h>
#include <World/Scene/SceneUpdateContext.h>
#include <Content/AssetCatalog.h>
#include <map>
#include <tuple>

namespace KT::RuntimeIntegration
{
	// 完成CPU入力からSkin行列を値コピーする。World/GPUへ書き戻さない。
	class AnimationExtraction
	{
	public:
		void Apply(const KT::World::SceneUpdateContext& cpu, const KT::Content::AssetCatalog& assets, RenderFrame& frame);

	private:
		// World・slot・世代・素材を区別し、Entity再利用やScene切替でContextを混ぜない。
		using SamplerKey = std::tuple<std::uint64_t, std::uint32_t, std::uint64_t, std::string>;
		std::map<KT::Content::AssetId, std::shared_ptr<const KT::Animation::SkeletonAnimation>> animations_;
		std::map<SamplerKey, std::unique_ptr<KT::Animation::PoseSampler>> samplers_;
	};
}
