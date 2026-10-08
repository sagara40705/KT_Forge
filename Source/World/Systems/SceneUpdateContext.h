#pragma once
#include <World/SceneData.h>

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

	// 一度捕捉した入力と、その更新の派生結果を値として所有する。World参照は保持しない。
	// 各Systemだけが対応結果を書き、別World/別更新のsnapshotを混ぜる入口を作らない。
	// 固定段階の検査は順序違い/失敗後の続行を防ぐため。計算失敗時は新contextからやり直す。
	class SceneUpdateContext : private KT::Core::NonCopyable
	{
	public:
		enum class Stage { Captured, Hierarchy, Activation, Transform, Camera, Failed };
		SceneUpdateContext(const World& world, std::optional<Entity> camera, Viewport viewport);
		Stage CurrentStage() const noexcept { return stage_; }
		std::uint64_t SourceWorldId() const noexcept { return worldId_; }
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
		void Fail() noexcept { stage_ = Stage::Failed; }
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
