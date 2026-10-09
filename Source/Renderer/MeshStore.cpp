#include <Renderer/MeshStore.h>
#include <Graphics/GraphicsValidation.h>
#include <stdexcept>
#include <utility>

namespace KT::Renderer
{
	const Mesh& MeshStore::Add(std::uint64_t id, std::unique_ptr<Mesh> mesh)
	{
		if (id==0 || !mesh || meshes_.contains(id)) throw std::invalid_argument("Mesh ID is zero/duplicate or Mesh is null.");
		KT::Graphics::RequireSameDevice(mesh->GetVertices().GetResource(),device_.GetDevice());
		const auto [it,added]=meshes_.emplace(id,std::move(mesh));
		(void)added;
		return *it->second;
	}
	const Mesh& MeshStore::Get(std::uint64_t id) const { return *meshes_.at(id); }
}
