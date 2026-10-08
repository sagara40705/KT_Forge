#pragma once
#include <Renderer/UnlitMaterial.h>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace KT::Renderer
{
	// pipelineを1個所有、各nonzero IDのmaterialを所有。GPU完了まで全体を保持。
	class MaterialStore : private KT::Core::NonCopyable
	{
	public:
		MaterialStore(KT::Graphics::GraphicsDevice& device, DXGI_FORMAT target=DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT depth=DXGI_FORMAT_D32_FLOAT) : pipeline_(device,target,depth) {}
		const UnlitMaterial& AddUnlit(std::uint64_t id, std::array<float,4> color);
		const UnlitMaterial& Get(std::uint64_t id) const;
	private:
		UnlitPipeline pipeline_;
		std::unordered_map<std::uint64_t,std::unique_ptr<UnlitMaterial>> materials_;
	};
}
