#pragma once
#include <Graphics/Pipeline/GraphicsPipelineState.h>

namespace KT::Renderer
{
	// 二段階照明の静的/Skin PSO。t0色とt1影は同じtableで借用する。
	class ToonPipeline : private KT::Core::NonCopyable
	{
	public:
		ToonPipeline(KT::Graphics::GraphicsDevice& device, DXGI_FORMAT target, DXGI_FORMAT depth, bool skinned);

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
