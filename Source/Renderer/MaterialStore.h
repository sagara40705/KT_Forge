#pragma once
#include <Renderer/UnlitMaterial.h>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace KT::Renderer
{
	// 単色/Textureのpipelineを所有、各nonzero IDのmaterialを所有。GPU完了まで全体を保持。
	class MaterialStore : private KT::Core::NonCopyable
	{
	public:
		MaterialStore(KT::Graphics::GraphicsDevice& device, DXGI_FORMAT target = DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT depth = DXGI_FORMAT_D32_FLOAT)
			: device_(device),
			  pipeline_(device, target, depth),
			  texturedPipeline_(device, target, depth, true),
			  skinnedPipeline_(device, target, depth, false, true),
			  texturedSkinnedPipeline_(device, target, depth, true, true)
		{
		}

		const UnlitMaterial& AddUnlit(std::uint64_t id, std::array<float, 4> color);
		const UnlitMaterial& AddUnlit(
			std::uint64_t id, const KT::Content::MaterialAsset& material, const KT::Graphics::SampledTexture* texture);
		const UnlitMaterial& Get(std::uint64_t id) const;

	private:
		KT::Graphics::GraphicsDevice& device_;
		UnlitPipeline pipeline_, texturedPipeline_;
		// 静的Materialと設定を共有する、単色とTextureのSkin描画経路。
		UnlitPipeline skinnedPipeline_, texturedSkinnedPipeline_;
		// Point/LinearとUV各軸のWrap/Clampの8通りを必要時だけ生成する。
		std::array<std::unique_ptr<KT::Graphics::Sampler>, 8> samplers_{};
		std::unordered_map<std::uint64_t, std::unique_ptr<UnlitMaterial>> materials_;
	};
}
