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
		if (id == 0 || materials_.contains(id))
		{
			throw std::invalid_argument("Material IDが0または重複しています。");
		}
		const KT::Graphics::Sampler* sampler = nullptr;
		if (texture)
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
		auto material = std::make_unique<UnlitMaterial>(texture ? texturedPipeline_ : pipeline_, asset, texture, sampler);
		const auto [found, added] = materials_.emplace(id, std::move(material));
		(void)added;
		return *found->second;
	}

	const UnlitMaterial& MaterialStore::Get(std::uint64_t id) const
	{
		return *materials_.at(id);
	}
}
