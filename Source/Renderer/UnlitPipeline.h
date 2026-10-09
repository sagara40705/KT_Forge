#pragma once
#include <Graphics/Pipeline/GraphicsPipelineState.h>

namespace KT::Renderer
{
	// unlit 3D用RootCBV b0=View,b1=Object,b2=Material。store内の全materialで1個のPSOを共有。
	class UnlitPipeline : private KT::Core::NonCopyable
	{
	public:
		UnlitPipeline(KT::Graphics::GraphicsDevice& device, DXGI_FORMAT target = DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT depth = DXGI_FORMAT_D32_FLOAT);

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
