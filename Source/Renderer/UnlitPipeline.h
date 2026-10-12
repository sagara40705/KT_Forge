#pragma once
#include <Graphics/Pipeline/GraphicsPipelineState.h>

namespace KT::Renderer
{
	// Unlitと深度専用ShadowのPSO。target=UNKNOWNは影を選ぶ。Skin版はb3、Texture版はt0/s0を追加する。
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
