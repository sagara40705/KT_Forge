#pragma once
#include <Renderer/UnlitPipeline.h>
#include <array>

namespace KT::Renderer
{
	// immutable色と共有pipelineの非所有借用。pipelineはmaterialより長生きする。
	class UnlitMaterial
	{
	public:
		UnlitMaterial(const UnlitPipeline& pipeline, std::array<float,4> color);
		const std::array<float,4>& GetColor() const noexcept { return color_; }
		const UnlitPipeline& GetPipeline() const noexcept { return pipeline_; }
	private:
		const UnlitPipeline& pipeline_;
		std::array<float,4> color_;
	};
}
