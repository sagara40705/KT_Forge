#pragma once
#include <World/Scene/SceneData.h>

namespace KT::World
{
	class HierarchySystem;
	class ActivationSystem;
	class TransformSystem;
	class CameraSystem;

	// 一つのEntityからコピーしたCPU入力。componentへの参照は保持しない。
	struct SceneEntityInput
	{
		Entity entity;
		Hierarchy hierarchy{};
		ActiveSelf activeSelf{};
		// 親からの変換を計算する前の、ローカル座標系の入力値。
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
		// 完了した更新段階。Failedになったcontextは再利用しない。
		enum class Stage
		{
			Captured,
			Hierarchy,
			Activation,
			Transform,
			Camera,
			Failed
		};

		SceneUpdateContext(const World& world, std::optional<Entity> camera, Viewport viewport, std::uint64_t updateNumber = 0);

		[[nodiscard]] std::uint64_t UpdateNumber() const noexcept
		{
			return updateNumber_;
		}

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
		// Cameraの実行なしでCPU計算を利用できる。結果はこのcontextが所有する。
		const WorldFrame& GetCpuFrame() const;

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

		// 入力を捕捉したWorldの識別値。World自体は借用しない。
		std::uint64_t worldId_;
		// Sceneが発行したCPU更新番号。直接構築した場合の既定値は0。
		std::uint64_t updateNumber_;
		// CameraSystem::Updateで使うCameraの指定。CPU計算だけなら未指定。
		std::optional<Entity> camera_;
		// Camera計算に使う描画領域。CPU階層・Transform計算では使用しない。
		Viewport viewport_;
		// 捕捉時のスロット順で保持し、全派生結果のindexと対応させる。
		std::vector<SceneEntityInput> inputs_;
		// 検証済みの親子関係と、親から処理する順序。
		HierarchySnapshot hierarchy_;
		// 祖先の有効状態を反映した、Entityごとの最終有効状態。
		std::vector<bool> active_;
		// CPU派生結果と、任意のCamera計算結果を所有する。
		WorldFrame frame_;
		// 結果の参照可否と、Systemの実行順を検査する状態。
		Stage stage_ = Stage::Captured;
	};
}
