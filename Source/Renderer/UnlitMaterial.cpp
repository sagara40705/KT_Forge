#include <Renderer/UnlitMaterial.h>
#include <cmath>
#include <stdexcept>

namespace KT::Renderer
{
	UnlitMaterial::UnlitMaterial(const UnlitPipeline& pipeline, std::array<float, 4> color)
		: pipeline_(pipeline),
		  color_(color)
	{
		for (float value : color)
		{
			if (!std::isfinite(value) || value < 0 || value > 1)
			{
				throw std::invalid_argument("Unlit color must be finite RGBA in [0,1].");
			}
		}
	}
}
