#pragma once
#include <Renderer/Mesh.h>
#include <memory>
#include <unordered_map>

namespace KT::Renderer
{
	// nonzero uint64 ID→安定したMesh所有。削除APIなし、GPU完了までstoreを保持。
	class MeshStore : private KT::Core::NonCopyable
	{
	public:
		explicit MeshStore(KT::Graphics::GraphicsDevice& device)
			: device_(device)
		{
		}

		const Mesh& Add(std::uint64_t id, std::unique_ptr<Mesh> mesh);
		const Mesh& Get(std::uint64_t id) const;

	private:
		KT::Graphics::GraphicsDevice& device_; // 登録resourceのdevice検査用の非所有借用。
		std::unordered_map<std::uint64_t, std::unique_ptr<Mesh>> meshes_;
	};
}
