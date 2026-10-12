#include <World/Scene/Scene.h>
#include <World/Systems/HierarchySystem.h>
#include <World/Systems/ActivationSystem.h>
#include <World/Systems/TransformSystem.h>
#include <World/Physics/PhysicsComponents.h>
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

	void Scene::AttachPhysics(std::unique_ptr<ScenePhysics> physics)
	{
		if (!HasSnapshot() || IsUpdating() || valueEdits_ != 0 || commands_.PendingCount() != 0 || physics_ || !physics)
		{
			throw std::logic_error("物理接続には未接続で予約のない完成Sceneが必要です。");
		}
		physics_ = std::move(physics);
	}

	ScenePhysics& Scene::Physics()
	{
		return const_cast<ScenePhysics&>(std::as_const(*this).Physics());
	}

	const ScenePhysics& Scene::Physics() const
	{
		if (!physics_ || state_ == State::Failed || (IsUpdating() && !gamePhase_))
		{
			throw std::logic_error("Sceneの物理が未接続、失敗中、または参照できない更新段階です。");
		}
		return *physics_;
	}

	void Scene::SetPhysicsSettings(const KT::Physics::SceneSettings& settings)
	{
		if (state_ != State::Ready || physics_ || valueEdits_ != 0)
		{
			throw std::logic_error("Sceneの物理設定は物理接続前の待機中だけ変更できます。");
		}
		KT::Physics::Validate(settings);
		physicsSettings_ = settings;
		explicitPhysicsSettings_ = true;
	}

	void Scene::WritePhysics(std::span<const PhysicsWriteback> values)
	{
		// 専用段階だけでjournalを取って書き戻す。ゲーム用の編集許可は広げない。
		for (const auto& value : values)
		{
			world_.BackupComponent<LocalTransform>(value.entity);
			world_.BackupComponent<RigidBody>(value.entity);
			auto& transform = world_.GetEditableComponent<LocalTransform>(value.entity);
			transform.position = value.state.pose.position;
			transform.rotation = value.state.pose.rotation;
			auto* body = World::FindIn<RigidBody>(*world_.slots_[value.entity.index].data);
			body->state_ = value.state;
		}
	}

	void Scene::RequireValueEditing() const
	{
		world_.RequireReadable();
		if (state_ == State::Failed || (state_ == State::Updating && !gamePhase_))
		{
			throw std::logic_error("Sceneの失敗後、またはゲーム更新以外の更新処理中はcomponentを編集できません。");
		}
	}

	void Scene::RecycleCpu(std::unique_ptr<SceneUpdateContext> context) noexcept
	{
		for (auto& scratch : cpuScratch_)
		{
			if (!scratch)
			{
				scratch = std::move(context);
				return;
			}
		}
	}

	std::unique_ptr<SceneUpdateContext> Scene::ComputeCpu(std::uint64_t updateNumber, const SceneUpdateContext* previous)
	{
		// Worldを一度捕捉し、階層・有効状態・World行列をこの順に確定する。
		std::unique_ptr<SceneUpdateContext> context;
		for (auto& scratch : cpuScratch_)
		{
			if (scratch)
			{
				context = std::move(scratch);
				break;
			}
		}
		try
		{
			if (context)
			{
				context->Capture(world_, std::nullopt, Viewport{}, updateNumber, previous);
			}
			else
			{
				context = std::make_unique<SceneUpdateContext>(world_, std::nullopt, Viewport{}, updateNumber, previous);
			}
			HierarchySystem{}.Update(*context);
			ActivationSystem{}.Update(*context);
			TransformSystem{}.Update(*context);
			return context;
		}
		catch (...)
		{
			RecycleCpu(std::move(context));
			throw;
		}
	}

	void Scene::ValidateUpdateStart(double deltaSeconds) const
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
		if (physics_ && std::abs(deltaSeconds - KT::Physics::FixedDeltaSeconds) > 1.0e-12)
		{
			throw std::invalid_argument("物理接続済みSceneは1/60秒の固定tickで更新してください。");
		}

		if (updateNumber_ == (std::numeric_limits<std::uint64_t>::max)())
		{
			throw std::overflow_error("Sceneの更新番号が上限に達しました。");
		}
	}

	const SceneUpdateContext& Scene::Update(double deltaSeconds, const GameUpdate& gameUpdate)
	{
		ValidateUpdateStart(deltaSeconds);

		// 更新中は旧結果を隠して保持し、今回の結果が完成するまで変更を仮反映する。
		state_ = State::Updating;
		auto previousSnapshot = std::move(snapshot_);
		const bool previousSnapshotValid = snapshotValid_;
		snapshotValid_ = false;
		++updateNumber_;
		std::unique_ptr<SceneUpdateContext> input;
		try
		{
			// 境界診断の再初期化も、今回の更新を復元する区間へ含める。
			commandResults_ = {};
			world_.BeginTransaction();
			// 開始境界の予約を反映し、今回のゲーム更新で使う固定CPU入力を作る。
			commandResults_[0] = commands_.Flush();
			if (!commandResults_[0].Succeeded())
			{
				std::rethrow_exception(commandResults_[0].error);
			}

			input = ComputeCpu(updateNumber_, previousSnapshot.get());
			if (physics_)
			{
				physics_->Begin(world_, *input);
			}
			else
			{
				world_.ForEachCandidate<RigidBody>(
					[](Entity) { throw std::logic_error("RigidBodyを更新する前にSceneの物理接続を準備してください。"); });
				world_.ForEachCandidate<Collider>(
					[](Entity) { throw std::logic_error("Colliderを更新する前にSceneの物理接続を準備してください。"); });
			}

			// Scriptを持つEntityだけをslot順で収集し、更新前に全対象を確定する。
			// 構造変更は後段のFlushまで反映せず、捕捉中はWorldへの再入を拒否する。
			scriptScratch_.clear();
			const auto& hierarchy = input->GetHierarchy();
			const auto& transforms = input->GetTransforms();
			world_.ForEachCandidate<ScriptComponent>([&](Entity entity)
			{
				const auto& component = std::as_const(world_).GetComponent<ScriptComponent>(entity);
				const bool active = transforms[hierarchy.FindNode(entity)].active.value;
				for (const auto& entry : component.entries_)
				{
					// 無効なものも含め、ゲーム処理で参照できる全実体を更新前に捕捉する。
					// 開始反映で除去した実体は未更新のままjournalが保持している。
					if (entry.instance)
					{
						world_.BackupScript(*entry.instance);
					}
					if (active && entry.definition.enabled && entry.instance)
					{
						scriptScratch_.emplace_back(entity, entry.instance.get());
					}
				}
			});

			// Scriptを対象確定時の順序で呼び、その後にゲーム固有の更新を行う。
			gamePhase_ = true;
			gameInput_ = input.get();
			for (const auto& [entity, script] : scriptScratch_)
			{
				script->Update(*this, entity, deltaSeconds, *input);
			}

			if (gameUpdate)
			{
				gameUpdate(*this, *input, deltaSeconds);
			}

			gamePhase_ = false;
			gameInput_ = nullptr;
			scriptScratch_.clear();
			// 開始時に有効だった個体の再生時計を進め、Sceneのtransactionへ含める。
			world_.ForEachCandidate<Animator>([&](Entity entity)
			{
				if (transforms[hierarchy.FindNode(entity)].active.value)
				{
					world_.BackupComponent<Animator>(entity);
					world_.GetEditableComponent<Animator>(entity).Advance(deltaSeconds);
				}
			});
			if (physics_)
			{
				WritePhysics(physics_->Simulate(world_));
			}

			// 終了境界の予約と値編集を反映し、再計算が成功した結果だけを公開する。
			commandResults_[1] = commands_.Flush();
			if (!commandResults_[1].Succeeded())
			{
				std::rethrow_exception(commandResults_[1].error);
			}

			auto completedSnapshot = ComputeCpu(updateNumber_, input.get());
			if (physics_)
			{
				physics_->Finish(world_, *completedSnapshot);
			}
			world_.CommitTransaction();
			if (physics_)
			{
				physics_->Commit();
			}
			snapshot_ = std::move(completedSnapshot);
			snapshotValid_ = true;
			state_ = State::Ready;
			// commit後だけ旧公開結果とゲーム入力を作業用へ戻す。rollback基準は上書きしない。
			RecycleCpu(std::move(previousSnapshot));
			RecycleCpu(std::move(input));
			return *snapshot_;
		}
		catch (...)
		{
			// 元の実体・値・CPU結果を戻す。失敗中の予約は捨て、再開は明示操作に限定する。
			const auto updateError = std::current_exception();
			gamePhase_ = false;
			gameInput_ = nullptr;
			scriptScratch_.clear();
			state_ = State::Failed;
			commands_.accepting_ = false;
			if (physics_)
			{
				physics_->Rollback();
			}
			world_.RollbackTransaction();
			RecycleCpu(std::move(input));
			snapshot_ = std::move(previousSnapshot);
			snapshotValid_ = previousSnapshotValid;
			for (auto& result : commandResults_)
			{
				result.rolledBack = true;
				result.created.clear();
				result.createdByCommand_.clear();
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
			if (const auto entity = result.FindCreated(reservation); entity && world_.IsAlive(*entity))
			{
				return *entity;
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
			snapshotValid_ = false;
		}
		world_.BackupComponent<ScriptComponent>(entity);
		component->entries_[scriptIndex].definition.enabled = enabled;
	}
}
