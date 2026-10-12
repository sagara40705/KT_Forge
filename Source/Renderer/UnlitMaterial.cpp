#include <Renderer/UnlitMaterial.h>
#include <stdexcept>

namespace KT::Renderer
{
	UnlitMaterial::UnlitMaterial(const UnlitPipeline& pipeline, std::array<float, 4> color)
		: UnlitMaterial(pipeline, KT::Content::MaterialAsset(color), nullptr, nullptr)
	{
	}

	UnlitMaterial::UnlitMaterial(const UnlitPipeline& pipeline, const KT::Content::MaterialAsset& material,
		const KT::Graphics::SampledTexture* texture, const KT::Graphics::Sampler* sampler, const UnlitPipeline* skinnedPipeline)
		: pipeline_(pipeline),
		  skinnedPipeline_(skinnedPipeline),
		  constants_{material.BaseColor()},
		  texture_(texture),
		  sampler_(sampler)
	{
		const auto& settings = material.Settings();
		if (skinnedPipeline && skinnedPipeline->GetPipeline().GetRootParameterCount() != (texture ? 6u : 4u))
		{
			throw std::invalid_argument("Skin MaterialのPipelineとTexture設定が一致しません。");
		}
		if (settings.baseColorTexture.has_value() != (texture != nullptr) || (texture == nullptr) != (sampler == nullptr) ||
			pipeline.GetPipeline().GetRootParameterCount() != (texture ? 5u : 3u))
		{
			throw std::invalid_argument("Unlit MaterialのTexture・Sampler・Pipelineの組合せが一致しません。");
		}
		constants_.uvScale = settings.uvScale;
		constants_.uvOffset = settings.uvOffset;
		constants_.alphaCutoff = settings.alphaMode == KT::Content::AlphaMode::Mask ? settings.alphaCutoff : -1.0f;
	}
}
