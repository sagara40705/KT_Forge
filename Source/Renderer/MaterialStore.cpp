#include <Renderer/MaterialStore.h>
#include <stdexcept>
#include <utility>

namespace KT::Renderer
{
	const UnlitMaterial& MaterialStore::AddUnlit(std::uint64_t id, std::array<float, 4> color)
	{
		return AddUnlit(id, KT::Content::MaterialAsset(color), nullptr);
	}

	const UnlitMaterial& MaterialStore::AddUnlit(
		std::uint64_t id, const KT::Content::MaterialAsset& asset, const KT::Graphics::SampledTexture* texture)
	{
		return AddSurface(id, asset, texture);
	}

	const UnlitMaterial& MaterialStore::AddSurface(
		std::uint64_t id, const KT::Content::MaterialAsset& asset, const KT::Graphics::SampledTexture* texture)
	{
		if (id == 0 || materials_.contains(id))
		{
			throw std::invalid_argument("Material IDが0または重複しています。");
		}
		const KT::Graphics::Sampler* sampler = nullptr;
		if (texture || asset.Model() == KT::Content::MaterialModel::Toon)
		{
			const auto& settings = asset.Settings();
			const bool linear = settings.filter == KT::Content::TextureFilter::Linear;
			const bool clampU = settings.addressU == KT::Content::TextureAddress::Clamp;
			const bool clampV = settings.addressV == KT::Content::TextureAddress::Clamp;
			const auto index = (linear ? 4 : 0) + (clampU ? 2 : 0) + (clampV ? 1 : 0);
			if (!samplers_[index])
			{
				samplers_[index] = std::make_unique<KT::Graphics::Sampler>(device_,
					linear ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_POINT,
					clampU ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP : D3D12_TEXTURE_ADDRESS_MODE_WRAP,
					clampV ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP : D3D12_TEXTURE_ADDRESS_MODE_WRAP);
			}
			sampler = samplers_[index].get();
		}
		auto material = std::make_unique<UnlitMaterial>(texture ? texturedPipeline_ : pipeline_, asset, texture, sampler,
			texture ? &texturedSkinnedPipeline_ : &skinnedPipeline_,
			asset.Model() == KT::Content::MaterialModel::Toon ? &toonPipeline_ : nullptr,
			asset.Model() == KT::Content::MaterialModel::Toon ? &toonSkinnedPipeline_ : nullptr,
			texture ? &shadowTexturedPipeline_ : &shadowPipeline_, texture ? &shadowTexturedSkinnedPipeline_ : &shadowSkinnedPipeline_);
		if (material->IsToon() && shadow_)
		{
			// Material公開前に対応するtableを用意し、例外時はどちらも未登録に戻す。
			auto table = std::make_unique<KT::Graphics::ColorDepthTextureTable>(device_, material->GetTexture(), *shadow_);
			texturePair_.emplace(id, std::move(table));
		}
		try
		{
			const auto [found, added] = materials_.emplace(id, std::move(material));
			(void)added;
			return *found->second;
		}
		catch (...)
		{
			texturePair_.erase(id);
			throw;
		}
	}

	const UnlitMaterial& MaterialStore::Get(std::uint64_t id) const
	{
		return *materials_.at(id);
	}

	void MaterialStore::BindShadow(KT::Graphics::PixelDepthView shadow)
	{
		std::unordered_map<std::uint64_t, std::unique_ptr<KT::Graphics::ColorDepthTextureTable>> pending;
		for (const auto& [id, material] : materials_)
		{
			if (material->IsToon())
			{
				pending.emplace(id, std::make_unique<KT::Graphics::ColorDepthTextureTable>(device_, material->GetTexture(), shadow));
			}
		}
		texturePair_.swap(pending);
		shadow_ = shadow;
	}

	const KT::Graphics::ColorDepthTextureTable* MaterialStore::GetToonTextures(std::uint64_t id) const
	{
		const auto found = texturePair_.find(id);
		return found == texturePair_.end() ? nullptr : found->second.get();
	}
}
