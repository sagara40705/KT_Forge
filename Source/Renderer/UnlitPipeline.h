#pragma once
#include <Graphics/Pipeline/GraphicsPipelineState.h>

namespace KT::Renderer
{
	// b0=View,b1=Object,b2=Material、Skin版はb3を追加。Textureはt0/s0、種類ごとにPSOを共有する。
	class UnlitPipeline : private KT::Core::NonCopyable
	{
	public:
		UnlitPipeline(KT::Graphics::GraphicsDevice& device, DXGI_FORMAT target = DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT depth = DXGI_FORMAT_D32_FLOAT, bool textured = false, bool skinned = false);

		const KT::Graphics::GraphicsPipelineState& GetPipeline() const noexcept
		{
			return pipeline_;
		}

	private:
		KT::Graphics::Shader vertex_, pixel_;
		KT::Graphics::RootSignature root_;
		KT::Graphics::GraphicsPipelineState pipeline_;
	};
}
