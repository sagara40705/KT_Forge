#pragma once
#include <Core/Math/Vector3.h>
#include <Core/Math/Quaternion.h>
#include <World/Entity.h>
#include <cstdint>

namespace KT::World
{
	// シーンの入力を保持する。派生結果はSceneDataへ分ける。
	// 階層とTransformは、直接編集した場合も描画前に全件検証する。
	// 欠落時は親なし・有効・単位Transformとして扱う。
	struct Hierarchy
	{
		Entity parent{};
	};

	struct ActiveSelf
	{
		bool value = true;
	};

	struct LocalTransform
	{
		KT::Core::Math::Vector3 position{};
		// xyzwを保持し、TransformSystemで入力を変えずに正規化する。
		KT::Core::Math::Quaternion rotation{};
		KT::Core::Math::Vector3 scale{1, 1, 1};
	};

	struct Camera
	{
		float verticalFovRadians = 1.0471975512f;
		float nearPlane = 0.1f;
		float farPlane = 1000.0f;
	};

	struct MeshRenderer
	{
		// 非0のアセットIDを保持する。アセットやGPU資源は所有しない。
		std::uint64_t meshId = 0;
		std::uint64_t materialId = 0;
		bool visible = true;
	};
}
