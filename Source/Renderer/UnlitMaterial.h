#pragma once
#include <Renderer/UnlitPipeline.h>
#include <array>
#include <stdexcept>
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
			const KT::Graphics::SampledTexture* texture, const KT::Graphics::Sampler* sampler,
			const UnlitPipeline* skinnedPipeline = nullptr);

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

		const UnlitPipeline& GetPipeline(bool skinned = false) const
		{
			if (skinned)
			{
				if (!skinnedPipeline_)
				{
					throw std::logic_error("MaterialにSkin用Pipelineが登録されていません。");
				}
				return *skinnedPipeline_;
			}
			return pipeline_;
		}

	private:
		const UnlitPipeline& pipeline_;
		// 同じMaterial設定をSkin頂点へ適用する共有Pipelineを借用する。
		const UnlitPipeline* skinnedPipeline_ = nullptr;
		MaterialConstants constants_;
		// Rendererのcacheから借用する。GPU完了までcacheが所有する。
		const KT::Graphics::SampledTexture* texture_ = nullptr;
		const KT::Graphics::Sampler* sampler_ = nullptr;
	};
}
