#pragma once
#include <Core/Math/Vector3.h>

#include <array>

namespace KT::World
{
	class ComponentRegistry;

	// 主光源のゲーム設定。directionは光が進むWorld方向で、EntityのTransformとは独立する。
	struct DirectionalLight
	{
		KT::Core::Math::Vector3 direction{0.4f, -1, 0.3f};
		// 主光源と環境の線形RGB。intensityは主光源だけに掛ける。
		std::array<float, 3> color{1, 1, 1};
		std::array<float, 3> ambient{0.15f, 0.15f, 0.18f};
		float intensity = 0.85f;
		bool enabled = true;
		bool shadows = true;
		// この中心をゲームからプレイヤーへ追従させる。範囲外は影なしとして描画する。
		KT::Core::Math::Vector3 shadowCenter{};
		float shadowExtent = 20;
		float shadowDepth = 40;
		unsigned shadowResolution = 1024;
		// Reverse-Zで受け側を光源へ近づける深度biasと、World単位の法線bias。
		float depthBias = 0.001f;
		float normalBias = 0.02f;
		float shadowStrength = 0.75f;

		void Validate() const;
	};

	// Sceneには設定だけを保存し、GPU資源を持ち込まない。
	void RegisterDirectionalLightComponent(ComponentRegistry& registry);
}
