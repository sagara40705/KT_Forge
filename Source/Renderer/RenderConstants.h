#pragma once
#include <Core/Math/Matrix4.h>
#include <array>
#include <type_traits>

namespace KT::Renderer
{
	// b0/b1/b2/b4のHLSL packing。各値は別の256byte領域に割り当てる。
	struct ViewConstants
	{
		KT::Core::Math::Matrix4 viewProjection;
	};

	struct ObjectConstants
	{
		KT::Core::Math::Matrix4 world;
		// Objectの逆転置。Toon以外では単位行列を保持する。
		KT::Core::Math::Matrix4 normalWorld;
	};

	struct MaterialConstants
	{
		std::array<float, 4> color;
		std::array<float, 2> uvScale{1, 1};
		std::array<float, 2> uvOffset{0, 0};
		// 負値はOpaque、0以上はMaskの閾値。
		float alphaCutoff = -1;
		std::array<float, 3> padding{};
		std::array<float, 3> shadeColor{0.45f, 0.45f, 0.55f};
		float threshold = 0.5f;
		float smoothness = 0.04f;
		float receiveShadows = 1;
		float hasTexture = 0;
		float toonPadding = 0;
	};

	// b4のPixel Shader用主光源。Lightなしは環境色1で従来の色を保つ。
	struct LightConstants
	{
		KT::Core::Math::Matrix4 shadowViewProjection;
		KT::Core::Math::Vector3 direction{0, -1, 0};
		float intensity = 0;
		std::array<float, 3> color{1, 1, 1};
		float shadowEnabled = 0;
		std::array<float, 3> ambient{1, 1, 1};
		float depthBias = 0.001f;
		float normalBias = 0.02f;
		float shadowStrength = 0.75f;
		std::array<float, 2> padding{};
	};

	static_assert(sizeof(LightConstants) == 128);
	static_assert(sizeof(ViewConstants) == 64 && sizeof(ObjectConstants) == 128 && sizeof(MaterialConstants) == 80);
	static_assert(std::is_trivially_copyable_v<ViewConstants> && std::is_trivially_copyable_v<ObjectConstants> &&
		std::is_trivially_copyable_v<MaterialConstants>);
}
