#pragma once
#include <Renderer/UnlitPipeline.h>
#include <array>
#include <Content/MaterialAsset.h>
#include <Renderer/RenderConstants.h>
#include <Graphics/Textures/SampledTexture.h>
#include <Graphics/Descriptors/Sampler.h>

namespace KT::Renderer
{
	// immutable色と共有pipelineの非所有借用。pipelineはmaterialより長生きする。
	class UnlitMaterial
	{
	public:
		UnlitMaterial(const UnlitPipeline& pipeline, std::array<float, 4> color);
		UnlitMaterial(const UnlitPipeline& pipeline, const KT::Content::MaterialAsset& material,
			const KT::Graphics::SampledTexture* texture, const KT::Graphics::Sampler* sampler);

		[[nodiscard]] const MaterialConstants& GetConstants() const noexcept
		{
			return constants_;
		}

		[[nodiscard]] const KT::Graphics::SampledTexture* GetTexture() const noexcept
		{
			return texture_;
		}

		[[nodiscard]] const KT::Graphics::Sampler* GetSampler() const noexcept
		{
			return sampler_;
		}

		const std::array<float, 4>& GetColor() const noexcept
		{
			return constants_.color;
		}

		const UnlitPipeline& GetPipeline() const noexcept
		{
			return pipeline_;
		}

	private:
		const UnlitPipeline& pipeline_;
		MaterialConstants constants_;
		// Rendererのcacheから借用する。GPU完了までcacheが所有する。
		const KT::Graphics::SampledTexture* texture_ = nullptr;
		const KT::Graphics::Sampler* sampler_ = nullptr;
	};
}
