#include <World/Scene/SceneHost.h>
#include <cmath>

namespace KT::World
{
	SceneHost::~SceneHost()
	{
		// 更新中の破棄で、Sceneやゲームcallbackの借用参照を失効させない。
		if (updating_ || changing_)
		{
			std::terminate();
		}
		// 自動破棄でも所有を先に空にし、Script破棄中の再入を拒否する。
		Clear();
	}

	void SceneHost::QueueScene(std::unique_ptr<Scene> scene)
	{
		if (changing_)
		{
			throw std::logic_error("Scene所有の交換中は次のSceneを予約できません。");
		}
		if (pending_ && (pending_->IsUpdating() || pending_->valueEdits_ != 0))
		{
			throw std::logic_error("予約候補のSceneが更新・編集中のため、置き換えられません。");
		}
		// 完成したCPU結果を持つ候補だけを受け取り、次の更新境界まで所有する。
		if (!scene || !scene->HasSnapshot() || scene->commands_.PendingCount() != 0)
		{
			throw std::invalid_argument("切替候補には、CPU検証済みで未反映の予約コマンドがないSceneを指定してください。");
		}

		// 新しい所有を公開してから旧候補を破棄し、失効したpointerを公開しない。
		auto retired = std::move(pending_);
		pending_ = std::move(scene);
		changing_ = true;
		retired.reset();
		changing_ = false;
	}

	void SceneHost::ActivateQueuedScene()
	{
		if (changing_ || updating_ || (active_ && (active_->IsUpdating() || active_->valueEdits_ != 0)))
		{
			throw std::logic_error("Scene更新中は表示用のScene切替を行えません。");
		}
		if (!pending_ || !pending_->HasSnapshot() || pending_->commands_.PendingCount() != 0)
		{
			throw std::logic_error("表示用のScene切替には、予約のない完成候補が必要です。");
		}

		// 固定tickの開始条件だけを検査し、読込時に完成したsnapshotをそのまま使う。
		pending_->ValidateUpdateStart(KT::Physics::FixedDeltaSeconds);
		auto retired = std::move(active_);
		active_ = std::move(pending_);
		changing_ = true;
		retired.reset();
		changing_ = false;
	}

	void SceneHost::Clear()
	{
		// 編集callbackからも拒否し、検査に失敗した場合は両Sceneを保持する。
		if (changing_ || updating_ || (active_ && (active_->IsUpdating() || active_->valueEdits_ != 0)) ||
			(pending_ && (pending_->IsUpdating() || pending_->valueEdits_ != 0)))
		{
			throw std::logic_error("Sceneの更新・編集・所有交換中はSceneHostを解放できません。");
		}

		auto active = std::move(active_);
		auto pending = std::move(pending_);
		changing_ = true;
		pending.reset();
		active.reset();
		changing_ = false;
	}

	const SceneUpdateContext& SceneHost::Update(double deltaSeconds, const Scene::GameUpdate& gameUpdate)
	{
		// 再入と経過時間を検査し、失敗時はSceneを切り替えない。
		if (changing_ || updating_ || (active_ && (active_->IsUpdating() || active_->valueEdits_ != 0)))
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
			// 固定delta等の拒否で旧Sceneと予約候補を失わないよう、交換前に確認する。
			pending_->ValidateUpdateStart(deltaSeconds);
			auto retired = std::move(active_);
			active_ = std::move(pending_);
			changing_ = true;
			retired.reset();
			changing_ = false;
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
