#include <Renderer/MaterialStore.h>
#include <stdexcept>

namespace KT::Renderer
{
	const UnlitMaterial& MaterialStore::AddUnlit(std::uint64_t id, std::array<float,4> color)
	{
		if (id==0 || materials_.contains(id)) throw std::invalid_argument("Material ID is zero/duplicate.");
		auto material=std::make_unique<UnlitMaterial>(pipeline_,color);
		const auto [it,added]=materials_.emplace(id,std::move(material));
		(void)added;
		return *it->second;
	}
	const UnlitMaterial& MaterialStore::Get(std::uint64_t id) const { return *materials_.at(id); }
}
