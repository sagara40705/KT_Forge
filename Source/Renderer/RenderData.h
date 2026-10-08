#pragma once
#include <Core/Math/Matrix4.h>
#include <cstdint>
#include <vector>

namespace KT::Renderer
{
	struct RenderObject { std::uint64_t meshId = 0, materialId = 0; KT::Core::Math::Matrix4 world{}; };
	struct RenderWorld { std::vector<RenderObject> objects; };
	struct RenderView
	{
		KT::Core::Math::Matrix4 view{}, projection{}, viewProjection{};
		KT::Core::Math::Vector3 position{};
		std::uint32_t width = 0, height = 0;
	};
}
