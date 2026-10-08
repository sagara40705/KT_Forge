#pragma once
#include <Core/Math/Matrix4.h>
#include <World/Hierarchy.h>
#include <optional>

namespace KT::World
{
	struct ActiveInHierarchy { bool value = true; };
	struct WorldTransform { KT::Core::Math::Matrix4 matrix{}; };
	struct FinalizedEntity { Entity entity; ActiveInHierarchy active; WorldTransform transform; };
	struct Viewport { std::uint32_t width = 0, height = 0; };
	struct CameraViewData
	{
		Entity entity;
		KT::Core::Math::Matrix4 view{}, projection{}, viewProjection{};
		KT::Core::Math::Vector3 position{};
		Viewport viewport;
	};
	// CPU結果を値として所有する。component参照やRenderer依存を持たない。
	struct WorldFrame
	{
		std::vector<FinalizedEntity> entities;
		std::optional<CameraViewData> camera;
	};
}
