#include <World/Scene/SceneHost.h>
#include <cmath>

namespace KT::World
{
	SceneHost::~SceneHost()
	{
		// 更新中の破棄で、Sceneやゲームcallbackの借用参照を失効させない。
		if (updating_)
		{
			std::terminate();
		}
	}

	void SceneHost::QueueScene(std::unique_ptr<Scene> scene)
	{
		// 完成したCPU結果を持つ候補だけを受け取り、次の更新境界まで所有する。
		if (!scene || !scene->HasSnapshot() || scene->commands_.PendingCount() != 0)
		{
			throw std::invalid_argument("切替候補には、CPU検証済みで未反映の予約コマンドがないSceneを指定してください。");
		}

		pending_ = std::move(scene);
	}

	const SceneUpdateContext& SceneHost::Update(double deltaSeconds, const Scene::GameUpdate& gameUpdate)
	{
		// 再入と経過時間を検査し、失敗時はSceneを切り替えない。
		if (updating_ || (active_ && active_->IsUpdating()))
		{
			throw std::logic_error("SceneHostまたは使用中のSceneが更新中のため、Updateへ再入できません。");
		}

		if (!std::isfinite(deltaSeconds) || deltaSeconds < 0)
		{
			throw std::invalid_argument("更新の経過時間は有限かつ0以上である必要があります。");
		}

		// 候補の完成状態を再確認し、開始境界でSceneの所有を入れ替える。
		if (pending_)
		{
			if (!pending_->HasSnapshot() || pending_->commands_.PendingCount() != 0)
			{
				throw std::logic_error("切替候補のSceneが検証後に変更され、完成したCPU結果がないか未反映の予約コマンドがあります。");
			}
			active_ = std::move(pending_);
		}

		if (!active_)
		{
			throw std::logic_error("SceneHostに更新対象のSceneがありません。");
		}

		// Hostの再入を防ぎ、Scene更新の例外時もHostの更新状態を解除する。
		updating_ = true;
		try
		{
			const auto& result = active_->Update(deltaSeconds, gameUpdate);
			updating_ = false;
			return result;
		}
		catch (...)
		{
			updating_ = false;
			throw;
		}
	}
}
