#pragma once
#include <Renderer/RenderConstants.h>

namespace KT::Renderer
{
	// RuntimeIntegrationが完成させた主光源の値。WorldやGPU資源を参照しない。
	struct SceneLighting
	{
		// 検証済みのb4値と、Rendererが確保する影画像の寸法。
		LightConstants constants;
		unsigned shadowResolution = 1024;
		bool shadows = false;
		void Validate() const;
	};

	// 影の中心を原点にしたLH直交Camera。有限far Reverse-Zでnear=1、far=0。
	KT::Core::Math::Matrix4 DirectionalShadowMatrix(
		KT::Core::Math::Vector3 direction, KT::Core::Math::Vector3 center, float extent, float depth, unsigned resolution);
}
