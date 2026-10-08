#pragma once
#include <array>
#include <cstddef>
#include <type_traits>

namespace KT::Graphics
{
	// 三角形部品専用の頂点。positionはclip空間、colorはRGB。
	// 行列・カメラ・ゲーム全体の座標規約を決める型ではない。
	struct TriangleVertex
	{
		float position[3];
		float color[3];
	};
	static_assert(std::is_standard_layout_v<TriangleVertex>);
	static_assert(std::is_trivially_copyable_v<TriangleVertex>);
	static_assert(sizeof(TriangleVertex) == 24);
	static_assert(offsetof(TriangleVertex, position) == 0);
	static_assert(offsetof(TriangleVertex, color) == 12);

	inline constexpr std::array<TriangleVertex, 3> TriangleVertices{{
		{{0.0f, 0.65f, 0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.65f, -0.65f, 0.5f}, {0.0f, 1.0f, 0.0f}},
		{{-0.65f, -0.65f, 0.5f}, {0.0f, 0.0f, 1.0f}}
	}};
}
