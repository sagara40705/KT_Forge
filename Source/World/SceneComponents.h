#pragma once
#include <Core/Math/Vector3.h>
#include <Core/Math/Quaternion.h>
#include <World/Entity.h>
#include <cstdint>

namespace KT::World
{
	// 入力のみ。欠落Hierarchy=root、ActiveSelf=true、LocalTransform=identity。
	// 直接編集も可能だが、描画前のHierarchySystem/TransformSystemで全件検証する。
	struct Hierarchy { Entity parent{}; };
	struct ActiveSelf { bool value = true; };
	struct LocalTransform
	{
		KT::Core::Math::Vector3 position{};
		KT::Core::Math::Quaternion rotation{}; // xyzw、TransformSystemで正規化、入力は変更しない。
		KT::Core::Math::Vector3 scale{1,1,1};
	};
	struct Camera
	{
		float verticalFovRadians = 1.0471975512f;
		float nearPlane = 0.1f;
		float farPlane = 1000.0f;
	};
	struct MeshRenderer
	{
		std::uint64_t meshId = 0, materialId = 0; // 非0の識別情報のみ。asset/GPU所有なし。
		bool visible = true;
	};
}
