#pragma once
#include <Renderer/UnlitMaterial.h>
#include <cstdint>
#include <Graphics/Textures/ColorDepthTextureTable.h>
#include <optional>
#include <memory>
#include <unordered_map>

namespace KT::Renderer
{
	// Unlit/Toon/Shadow PSOとMaterial別のtableを所有。同期1slotのGPU完了まで全体を保持。
	class MaterialStore : private KT::Core::NonCopyable
	{
	public:
		MaterialStore(KT::Graphics::GraphicsDevice& device, DXGI_FORMAT target = DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT depth = DXGI_FORMAT_D32_FLOAT)
			: device_(device),
			  pipeline_(device, target, depth),
			  texturedPipeline_(device, target, depth, true),
			  skinnedPipeline_(device, target, depth, false, true),
			  texturedSkinnedPipeline_(device, target, depth, true, true),
			  toonPipeline_(device, target, depth, false),
			  toonSkinnedPipeline_(device, target, depth, true),
			  shadowPipeline_(device, DXGI_FORMAT_UNKNOWN, depth),
			  shadowTexturedPipeline_(device, DXGI_FORMAT_UNKNOWN, depth, true),
			  shadowSkinnedPipeline_(device, DXGI_FORMAT_UNKNOWN, depth, false, true),
			  shadowTexturedSkinnedPipeline_(device, DXGI_FORMAT_UNKNOWN, depth, true, true)
		{
		}

		const UnlitMaterial& AddUnlit(std::uint64_t id, std::array<float, 4> color);
		const UnlitMaterial& AddUnlit(
			std::uint64_t id, const KT::Content::MaterialAsset& material, const KT::Graphics::SampledTexture* texture);
        // 制作向けの登録窓口。MaterialModelからUnlit/Toonを選ぶ。
        const UnlitMaterial& AddSurface(std::uint64_t id, const KT::Content::MaterialAsset& material,
            const KT::Graphics::SampledTexture* texture = nullptr);
        const UnlitMaterial& Get(std::uint64_t id) const;

		// 画像差替え時は先にReleaseFrameする。失敗時は旧table群を保持する。
		void BindShadow(KT::Graphics::PixelDepthView shadow);
		const KT::Graphics::ColorDepthTextureTable* GetToonTextures(std::uint64_t id) const;

	private:
		KT::Graphics::GraphicsDevice& device_;
		UnlitPipeline pipeline_, texturedPipeline_;
		// 静的Materialと設定を共有する、単色とTextureのSkin描画経路。
		UnlitPipeline skinnedPipeline_, texturedSkinnedPipeline_;
		// 主光源の共有Toon PSOと、可視MaterialのMaskを維持する深度専用PSO。
		ToonPipeline toonPipeline_, toonSkinnedPipeline_;
		UnlitPipeline shadowPipeline_, shadowTexturedPipeline_, shadowSkinnedPipeline_, shadowTexturedSkinnedPipeline_;
		// Point/LinearとUV各軸のWrap/Clampの8通りを必要時だけ生成する。
		std::array<std::unique_ptr<KT::Graphics::Sampler>, 8> samplers_{};
		std::unordered_map<std::uint64_t, std::unique_ptr<UnlitMaterial>> materials_;
		// 同じ影画像に対するMaterial別のt0/t1 table。画像自体の所有はRenderer。
		std::optional<KT::Graphics::PixelDepthView> shadow_;
		std::unordered_map<std::uint64_t, std::unique_ptr<KT::Graphics::ColorDepthTextureTable>> texturePair_;
	};
}
