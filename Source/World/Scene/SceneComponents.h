#pragma once
#include <Core/Math/Vector3.h>
#include <Core/Math/Quaternion.h>
#include <World/Entity.h>
#include <cstdint>

namespace KT::World
{
	// 同じWorld内の親を保持する。SetParentだけで編集し、欠落時は親なしとして扱う。
	struct Hierarchy
	{
		Entity parent{};
	};

	// 自身の有効設定を保持する。祖先を含む最終状態はCPU更新で求める。
	struct ActiveSelf
	{
		bool value = true;
	};

	// 親の座標系での変換入力。CPU更新で検証し、欠落時は単位変換として扱う。
	struct LocalTransform
	{
		KT::Core::Math::Vector3 position{};
		// xyzwを保持し、TransformSystemで入力を変えずに正規化する。
		KT::Core::Math::Quaternion rotation{};
		KT::Core::Math::Vector3 scale{1, 1, 1};
	};

	// Reverse-Z投影に使う透視Cameraの設定。距離は正の有限値を指定する。
	struct Camera
	{
		float verticalFovRadians = 1.0471975512f;
		float nearPlane = 0.1f;
		float farPlane = 1000.0f;
	};

	// 描画用のアセット参照と表示設定。描画用値へのコピーはRuntimeIntegrationが行う。
	struct MeshRenderer
	{
		// 非0のアセットIDを保持する。アセットやGPU資源は所有しない。
		std::uint64_t meshId = 0;
		std::uint64_t materialId = 0;
		bool visible = true;
	};
}
