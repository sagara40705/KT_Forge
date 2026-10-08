#pragma once
#include <Core/Math/Matrix4.h>
#include <array>
#include <type_traits>

namespace KT::Renderer
{
	// b0/b1/b2のHLSL packing。各値は別の256byte領域に割り当てる。
	struct ViewConstants { KT::Core::Math::Matrix4 viewProjection; };
	struct ObjectConstants { KT::Core::Math::Matrix4 world; };
	struct MaterialConstants { std::array<float,4> color; };
	static_assert(sizeof(ViewConstants)==64 && sizeof(ObjectConstants)==64 && sizeof(MaterialConstants)==16);
	static_assert(std::is_trivially_copyable_v<ViewConstants> && std::is_trivially_copyable_v<ObjectConstants> &&
		std::is_trivially_copyable_v<MaterialConstants>);
}
