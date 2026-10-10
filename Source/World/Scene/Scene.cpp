#include <World/Scene/Scene.h>
#include <World/Systems/HierarchySystem.h>
#include <World/Systems/ActivationSystem.h>
#include <World/Systems/TransformSystem.h>
#include <cmath>
#include <exception>

namespace KT::World
{
	Scene::Scene(std::string name)
		: name_(std::move(name)),
		  commands_(world_)
	{
	}

	Scene::~Scene()
	{
		// callback中の破棄を拒否し、Worldの破棄前に所有する予約を廃棄する。
		if (IsUpdating() || valueEdits_ != 0)
		{
			std::terminate();
		}

		commands_.accepting_ = false;
		commands_.Discard();
	}

	const World& Scene::GetWorld() const
	{
		return world_;
	}

	WorldCommandBuffer& Scene::Commands()
	{
		commands_.RequireAccepting();
		return commands_;
	}

	void Scene::RequireValueEditing() const
	{
		world_.RequireReadable();
		if (state_ == State::Failed || (state_ == State::Updating && !gamePhase_))
		{
			throw std::logic_error("Sceneの失敗後、またはゲーム更新以外の更新処理中はcomponentを編集できません。");
		}
	}

	std::unique_ptr<SceneUpdateContext> Scene::ComputeCpu(std::uint64_t updateNumber) const
	{
		// Worldを一度捕捉し、階層・有効状態・World行列をこの順に確定する。
		auto context = std::make_unique<SceneUpdateContext>(world_, std::nullopt, Viewport{}, updateNumber);
		HierarchySystem{}.Update(*context);
		ActivationSystem{}.Update(*context);
		TransformSystem{}.Update(*context);
		return context;
	}

	const SceneUpdateContext& Scene::Update(double deltaSeconds, const GameUpdate& gameUpdate)
	{
		// 更新開始条件と経過時間・番号を検査し、失敗時はWorldを変更しない。
		if (state_ != State::Ready || valueEdits_ != 0)
		{
			throw std::logic_error("Sceneは更新・値編集中、または失敗後の復旧待ちです。Updateを開始できません。");
		}

		if (!std::isfinite(deltaSeconds) || deltaSeconds < 0)
		{
			throw std::invalid_argument("更新の経過時間は有限かつ0以上である必要があります。");
		}

		if (updateNumber_ == (std::numeric_limits<std::uint64_t>::max)())
		{
			throw std::overflow_error("Sceneの更新番号が上限に達しました。");
		}

		// 更新中は旧結果を隠して保持し、今回の結果が完成するまで変更を仮反映する。
		state_ = State::Updating;
		auto previousSnapshot = std::move(snapshot_);
		commandResults_ = {};
		++updateNumber_;
		try
		{
			world_.BeginTransaction();
			// 開始境界の予約を反映し、今回のゲーム更新で使う固定CPU入力を作る。
			commandResults_[0] = commands_.Flush();
			if (!commandResults_[0].Succeeded())
			{
				std::rethrow_exception(commandResults_[0].error);
			}

			auto input = ComputeCpu(updateNumber_);

			// 更新対象を確定してから呼ぶ。生成・削除は後段のFlushまで反映しない。
			std::vector<std::pair<Entity, ScriptBehaviour*>> scripts;
			for (const auto& entity : input->GetTransforms())
			{
				const auto* component = world_.FindComponent<ScriptComponent>(entity.entity);
				if (component)
				{
					for (const auto& entry : component->entries_)
					{
						// 無効なものも含め、ゲーム処理で参照できる全実体を更新前に捕捉する。
						// 開始反映で除去した実体は未更新のままjournalが保持している。
						if (entry.instance)
						{
							world_.BackupScript(*entry.instance);
						}
						if (entity.active.value && entry.definition.enabled && entry.instance)
						{
							scripts.emplace_back(entity.entity, entry.instance.get());
						}
					}
				}
			}

			// Scriptを対象確定時の順序で呼び、その後にゲーム固有の更新を行う。
			gamePhase_ = true;
			gameInput_ = input.get();
			for (const auto& [entity, script] : scripts)
			{
				script->Update(*this, entity, deltaSeconds, *input);
			}

			if (gameUpdate)
			{
				gameUpdate(*this, *input, deltaSeconds);
			}

			gamePhase_ = false;
			gameInput_ = nullptr;

			// 終了境界の予約と値編集を反映し、再計算が成功した結果だけを公開する。
			commandResults_[1] = commands_.Flush();
			if (!commandResults_[1].Succeeded())
			{
				std::rethrow_exception(commandResults_[1].error);
			}

			auto completedSnapshot = ComputeCpu(updateNumber_);
			world_.CommitTransaction();
			snapshot_ = std::move(completedSnapshot);
			state_ = State::Ready;
			return *snapshot_;
		}
		catch (...)
		{
			// 元の実体・値・CPU結果を戻す。失敗中の予約は捨て、再開は明示操作に限定する。
			const auto updateError = std::current_exception();
			gamePhase_ = false;
			gameInput_ = nullptr;
			state_ = State::Failed;
			commands_.accepting_ = false;
			world_.RollbackTransaction();
			snapshot_ = std::move(previousSnapshot);
			for (auto& result : commandResults_)
			{
				result.rolledBack = true;
				result.created.clear();
				if (!result.error)
				{
					result.error = updateError;
				}
			}
			commands_.Discard();
			throw;
		}
	}

	const SceneUpdateContext& Scene::GetSnapshot() const
	{
		if (!HasSnapshot())
		{
			throw std::logic_error("Sceneに完成したCPUスナップショットがありません。");
		}
		return *snapshot_;
	}

	Entity Scene::Resolve(DeferredEntity reservation) const
	{
		// 今回の開始・終了境界で生成でき、まだ生存しているEntityだけを返す。
		for (const auto& result : commandResults_)
		{
			for (const auto& entry : result.created)
			{
				if (entry.reservation == reservation && world_.IsAlive(entry.entity))
				{
					return entry.entity;
				}
			}
		}
		throw std::invalid_argument("生成予約を解決できません。未反映・期限切れ、または生成したEntityが破棄済みです。");
	}

	std::optional<Entity> Scene::FindByUuid(ObjectUuid uuid) const
	{
		// 現在の仮反映も含むWorldの索引で検索し、更新境界前の結果を使い回さない。
		return world_.FindByUuid(uuid);
	}

	void Scene::ResetAfterFailure()
	{
		// 失敗状態からだけ復旧し、次のUpdateでWorldを再検証する。
		world_.RequireReadable();
		if (state_ != State::Failed || commands_.flushing_)
		{
			throw std::logic_error("Sceneが失敗状態でないか予約の反映中のため、ResetAfterFailureを実行できません。");
		}

		commands_.Discard();
		commands_.accepting_ = true;
		state_ = State::Ready;
	}

	void Scene::SetScriptEnabled(Entity entity, std::size_t scriptIndex, bool enabled)
	{
		// Sceneの編集段階、対象component、Scriptの位置を確認する。
		RequireValueEditing();
		(void)world_.GetComponent<ScriptComponent>(entity);
		auto* component = World::FindIn<ScriptComponent>(*world_.slots_[entity.index].data);
		if (scriptIndex >= component->entries_.size())
		{
			throw std::out_of_range("指定したScriptのindexがScriptComponentの範囲外です。");
		}

		// 待機中の完成結果を失効させ、有効設定を変更する。
		if (state_ == State::Ready)
		{
			snapshot_.reset();
		}
		world_.BackupComponent<ScriptComponent>(entity);
		component->entries_[scriptIndex].definition.enabled = enabled;
	}
}
