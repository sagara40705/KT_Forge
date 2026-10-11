#pragma once
#include <World/Scene/SceneComponents.h>
#include <Core/Math/Matrix4.h>

namespace KT::World
{
	// 行基底ごとの許容誤差で、平行移動が回転・scaleの誤差を隠さないように比較する。
	[[nodiscard]] bool TransformNear(const KT::Core::Math::Matrix4& first, const KT::Core::Math::Matrix4& second);
	// S*R*TのTRSへ分解する。shearや再構成不能な行列は近似せず拒否する。
	[[nodiscard]] LocalTransform DecomposeLocalTransform(const KT::Core::Math::Matrix4& matrix);
}