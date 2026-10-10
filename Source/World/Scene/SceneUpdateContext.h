#pragma once
#include <World/Scene/SceneData.h>

namespace KT::World
{
	class HierarchySystem;
	class ActivationSystem;
	class TransformSystem;
	class CameraSystem;

	struct SceneEntityInput
	{
		Entity entity;
		Hierarchy hierarchy{};
		ActiveSelf activeSelf{};
		LocalTransform local{};
		std::optional<Camera> camera;
		std::optional<MeshRenderer> mesh;
	};

	// World入力を一度コピーし、今回の派生結果とともに所有する。World参照は保持しない。
	// 対応するSystemだけが結果を更新し、別Worldや別更新の結果を混ぜない。
	// 更新順序と失敗後の続行を検査する。計算に失敗したら新しいcontextでやり直す。
	class SceneUpdateContext : private KT::Core::NonCopyable
	{
	public:
		enum class Stage
		{
			Captured,
			Hierarchy,
			Activation,
			Transform,
			Camera,
			Failed
		};

		SceneUpdateContext(const World& world, std::optional<Entity> camera, Viewport viewport);

		Stage CurrentStage() const noexcept
		{
			return stage_;
		}

		std::uint64_t SourceWorldId() const noexcept
		{
			return worldId_;
		}

		const std::vector<SceneEntityInput>& Inputs() const;

		// 結果配列のindexはInputs()と対応する。slotに穴がある場合のEntity.indexとは異なる。
		const HierarchySnapshot& GetHierarchy() const;
		const std::vector<bool>& GetActivation() const;
		const std::vector<FinalizedEntity>& GetTransforms() const;
		const WorldFrame& GetFrame() const;

	private:
		friend class HierarchySystem;
		friend class ActivationSystem;
		friend class TransformSystem;
		friend class CameraSystem;

		void RequireStage(Stage expected) const;
		void RequireAtLeast(Stage minimum) const;

		void Fail() noexcept
		{
			stage_ = Stage::Failed;
		}

		std::uint64_t worldId_;
		std::optional<Entity> camera_;
		Viewport viewport_;
		std::vector<SceneEntityInput> inputs_;
		HierarchySnapshot hierarchy_;
		std::vector<bool> active_;
		WorldFrame frame_;
		Stage stage_ = Stage::Captured;
	};
}
