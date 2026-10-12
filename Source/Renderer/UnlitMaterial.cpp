#include <Renderer/UnlitMaterial.h>
#include <stdexcept>

namespace KT::Renderer
{
	UnlitMaterial::UnlitMaterial(const UnlitPipeline& pipeline, std::array<float, 4> color)
		: UnlitMaterial(pipeline, KT::Content::MaterialAsset(color), nullptr, nullptr)
	{
	}

	UnlitMaterial::UnlitMaterial(const UnlitPipeline& pipeline, const KT::Content::MaterialAsset& material,
		const KT::Graphics::SampledTexture* texture, const KT::Graphics::Sampler* sampler, const UnlitPipeline* skinnedPipeline,
		const ToonPipeline* toonPipeline, const ToonPipeline* toonSkinnedPipeline, const UnlitPipeline* shadowPipeline,
		const UnlitPipeline* shadowSkinnedPipeline)
		: toonPipeline_(toonPipeline),
		  toonSkinnedPipeline_(toonSkinnedPipeline),
		  shadowPipeline_(shadowPipeline),
		  shadowSkinnedPipeline_(shadowSkinnedPipeline),
		  pipeline_(pipeline),
		  skinnedPipeline_(skinnedPipeline),
		  constants_{material.BaseColor()},
		  texture_(texture),
		  sampler_(sampler)
	{
		const auto& settings = material.Settings();
		if ((material.Model() == KT::Content::MaterialModel::Toon) != (toonPipeline != nullptr) ||
			(toonPipeline && (!sampler || !toonSkinnedPipeline)))
		{
			throw std::invalid_argument("Toon Materialの方式・Sampler・静的/Skin PSOが一致しません。");
		}
		const auto& toon = material.Toon();
		constants_.shadeColor = toon.shadeColor;
		constants_.threshold = toon.threshold;
		constants_.smoothness = toon.smoothness;
		constants_.receiveShadows = toon.receiveShadows ? 1.0f : 0.0f;
		constants_.hasTexture = texture ? 1.0f : 0.0f;
		if (skinnedPipeline && skinnedPipeline->GetPipeline().GetRootParameterCount() != (texture ? 6u : 4u))
		{
			throw std::invalid_argument("Skin MaterialのPipelineとTexture設定が一致しません。");
		}
		if (settings.baseColorTexture.has_value() != (texture != nullptr) ||
			(!toonPipeline && (texture == nullptr) != (sampler == nullptr)) ||
			pipeline.GetPipeline().GetRootParameterCount() != (texture ? 5u : 3u))
		{
			throw std::invalid_argument("Unlit MaterialのTexture・Sampler・Pipelineの組合せが一致しません。");
		}
		constants_.uvScale = settings.uvScale;
		constants_.uvOffset = settings.uvOffset;
		constants_.alphaCutoff = settings.alphaMode == KT::Content::AlphaMode::Mask ? settings.alphaCutoff : -1.0f;
	}

	const KT::Graphics::GraphicsPipelineState& UnlitMaterial::GetSurfacePipeline(bool skinned) const
	{
		if (toonPipeline_)
		{
			return (skinned ? toonSkinnedPipeline_ : toonPipeline_)->GetPipeline();
		}
		return GetPipeline(skinned).GetPipeline();
	}

	const KT::Graphics::GraphicsPipelineState& UnlitMaterial::GetShadowPipeline(bool skinned) const
	{
		const auto* pipeline = skinned ? shadowSkinnedPipeline_ : shadowPipeline_;
		if (!pipeline)
		{
			throw std::logic_error("MaterialにShadow用PSOが登録されていません。");
		}
		return pipeline->GetPipeline();
	}
}
