#pragma once
#include <Core/Math/Matrix4.h>
#include <World/Scene/Hierarchy.h>
#include <optional>

namespace KT::World
{
	// 更新で求めた有効状態とWorld行列を、入力componentとは別に保持する。
	struct ActiveInHierarchy
	{
		bool value = true;
	};

	struct WorldTransform
	{
		KT::Core::Math::Matrix4 matrix{};
	};

	struct FinalizedEntity
	{
		Entity entity;
		ActiveInHierarchy active;
		WorldTransform transform;
	};

	struct Viewport
	{
		std::uint32_t width = 0;
		std::uint32_t height = 0;
	};

	struct CameraViewData
	{
		Entity entity;
		KT::Core::Math::Matrix4 view{};
		KT::Core::Math::Matrix4 projection{};
		KT::Core::Math::Matrix4 viewProjection{};
		KT::Core::Math::Vector3 position{};
		Viewport viewport;
	};

	// CPU更新結果を値として所有する。component参照やRenderer依存は持たない。
	struct WorldFrame
	{
		std::vector<FinalizedEntity> entities;
		std::optional<CameraViewData> camera;
	};
}
