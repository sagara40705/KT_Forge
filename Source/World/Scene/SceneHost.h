#pragma once
#include <World/Scene/Scene.h>

namespace KT::World
{
	// 候補Sceneの読込後に切替を予約し、更新の開始境界で所有を入れ替える。
	class SceneHost : private KT::Core::NonCopyable
	{
	public:
		~SceneHost();
		void QueueScene(std::unique_ptr<Scene> scene);
		const SceneUpdateContext& Update(double deltaSeconds, const Scene::GameUpdate& gameUpdate = {});

		[[nodiscard]] Scene* GetScene() noexcept
		{
			return active_.get();
		}

		[[nodiscard]] const Scene* GetScene() const noexcept
		{
			return active_.get();
		}

	private:
		// 現在の更新対象を所有する。切替時に以前のSceneを破棄する。
		std::unique_ptr<Scene> active_;
		// 次の開始境界で使用する、検証済みの切替候補を所有する。
		std::unique_ptr<Scene> pending_;
		// Host自身の再入と、更新中の破棄を拒否する。
		bool updating_ = false;
	};
}
