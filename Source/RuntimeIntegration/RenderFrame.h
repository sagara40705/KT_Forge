#pragma once
#include <Renderer/RenderData.h>
#include <optional>

namespace KT::RuntimeIntegration
{
	struct RenderFrame
	{
		KT::Renderer::RenderWorld world;
		std::optional<KT::Renderer::RenderView> view;
	};
}
