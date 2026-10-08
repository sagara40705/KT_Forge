#include <BuiltinAssets/Materials/DefaultUnlitMaterial.h>

namespace KT::BuiltinAssets
{
	const KT::Renderer::UnlitMaterial& AddDefaultUnlitMaterial(KT::Renderer::MaterialStore& store, std::uint64_t id)
	{
		return store.AddUnlit(id,{1,1,1,1});
	}
}
