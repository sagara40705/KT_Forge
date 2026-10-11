#include <RuntimeIntegration/Physics/JoltScenePhysics.h>
#include <World/Physics/PhysicsComponents.h>
#include <algorithm>
#include <map>
#include <tuple>

namespace KT::RuntimeIntegration
{
	using namespace KT::World;
	using namespace KT::Physics;

	// World ID・index・generationを全て照合し、別Worldや再利用slotを混同しない。
	struct EntityLess
	{
		bool operator()(Entity first, Entity second) const noexcept
		{
			return std::tie(first.worldId, first.index, first.generation) < std::tie(second.worldId, second.index, second.generation);
		}
	};

	// 物理とWorldを一つの成功境界で公開するScene専用adapter。
	class JoltScenePhysics final : public ScenePhysics
	{
	public:
		JoltScenePhysics(const Scene& scene, PhysicsRuntime& runtime)
		{
			working_ = std::make_unique<PhysicsWorld>(runtime, scene.GetPhysicsSettings());
			Synchronize(scene.GetWorld(), scene.GetSnapshot());
			working_->Seal();
			Commit();
		}

		void Enqueue(Entity entity, const BodyCommand& command) override
		{
			if (!accepting_)
			{
				throw std::logic_error("物理命令はSceneのゲームcallback内だけで予約できます。");
			}
			Validate(command);
			const auto found = workingBodies_.find(entity);
			if (found == workingBodies_.end() || !working_->GetState(found->second.body).enabled)
			{
				throw std::invalid_argument("物理命令のEntityが失効しているか、開始境界で有効なBodyを持ちません。");
			}
			commands_.push_back({found->second.body, command});
		}

		std::optional<PhysicsRayHit> CastRay(const Ray& ray, std::optional<Entity> ignore) const override
		{
			const auto& bodies = accepting_ ? workingBodies_ : committedBodies_;
			const auto& physics = accepting_ ? working_ : committed_;
			Ray query = ray;
			if (ray.ignore)
			{
				throw std::invalid_argument("SceneのrayではBodyHandleの代わりにEntityをignoreへ指定してください。");
			}
			if (ignore)
			{
				const auto found = bodies.find(*ignore);
				if (found == bodies.end())
				{
					throw std::invalid_argument("rayから除外するEntityに対応したBodyがありません。");
				}
				query.ignore = found->second.body;
			}
			const auto hit = physics->CastRay(query);
			if (!hit)
			{
				return std::nullopt;
			}
			for (const auto& [entity, entry] : bodies)
			{
				if (entry.body == hit->body)
				{
					return PhysicsRayHit{entity, hit->position, hit->normal, hit->distance};
				}
			}
			throw std::logic_error("物理rayのBodyに対応するEntityがありません。");
		}

		std::span<const PhysicsContact> Contacts() const noexcept override
		{
			return accepting_ ? inputEvents_ : events_;
		}

	private:
		// 生存Entityのcomponent実体とBodyの対応。poseは直接編集の検出にも使う。
		struct Binding
		{
			BodyHandle body;
			// Worldのjournalは旧componentをcommitまで保持するため、置換をアドレスで識別できる。
			const RigidBody* rigidBody = nullptr;
			const Collider* collider = nullptr;
			Pose expectedPose;
		};

		void Begin(const KT::World::World& world, const SceneUpdateContext& cpu) override
		{
			// 前回失敗候補の破棄・確保・復元は次のtry区間で行う。rollbackは割当を行わない。
			working_.reset();
			workingBodies_ = committedBodies_;
			working_ = committed_->Clone();
			commands_.clear();
			pendingEvents_.clear();
			writebacks_.clear();
			Synchronize(world, cpu);
			inputEvents_.clear();
			for (const auto& contact : events_)
			{
				const auto valid = [&](Entity entity)
				{
					const auto current = workingBodies_.find(entity);
					const auto previous = committedBodies_.find(entity);
					return current != workingBodies_.end() && previous != committedBodies_.end() &&
						current->second.body == previous->second.body && working_->GetState(current->second.body).enabled;
				};
				if (valid(contact.first) && valid(contact.second))
				{
					inputEvents_.push_back(contact);
				}
			}
			accepting_ = true;
		}

		// 構造と姿勢の契約を先に確認し、候補側だけに追加・削除・有効変更を反映する。
		void Synchronize(const KT::World::World& world, const SceneUpdateContext& cpu)
		{
			std::map<Entity, Binding, EntityLess> next;
			for (const auto entity : world.Entities())
			{
				const auto* rigidBody = world.FindComponent<RigidBody>(entity);
				const auto* collider = world.FindComponent<Collider>(entity);
				if (!rigidBody && !collider)
				{
					continue;
				}
				const auto* transform = world.FindComponent<LocalTransform>(entity);
				const auto* hierarchy = world.FindComponent<Hierarchy>(entity);
				if (!rigidBody || !collider || !transform)
				{
					throw std::invalid_argument("物理EntityにはRigidBody・Collider・LocalTransformをそろえてください。");
				}
				if ((hierarchy && hierarchy->parent != Entity{}) || transform->scale != Vector3{1, 1, 1})
				{
					throw std::invalid_argument("物理Entityはroot・scale(1,1,1)に限定します。描画の拡縮は子Entityへ置いてください。");
				}
				const Pose pose{transform->position, transform->rotation};
				Validate(pose);
				const auto found = workingBodies_.find(entity);
				const bool same =
					found != workingBodies_.end() && found->second.rigidBody == rigidBody && found->second.collider == collider;
				if (same && found->second.expectedPose != pose)
				{
					throw std::invalid_argument(
						"物理EntityのLocalTransformを直接変更できません。TeleportまたはKinematicTargetを予約してください。");
				}
				next.emplace(entity, Binding{same ? found->second.body : BodyHandle{}, rigidBody, collider, pose});
			}

			// 全対象の検証後、削除・置換される旧Bodyを先に外して上限内の入替えを許可する。
			for (const auto& [entity, binding] : workingBodies_)
			{
				const auto found = next.find(entity);
				if (found == next.end() || found->second.body != binding.body)
				{
					working_->Remove(binding.body);
				}
			}

			// 空いたslotへ新Bodyを追加し、残したBodyは有効状態だけ同期する。
			for (auto& [entity, binding] : next)
			{
				const bool active = cpu.GetTransforms()[cpu.GetHierarchy().FindNode(entity)].active.value;
				if (binding.body == BodyHandle{})
				{
					binding.body = working_->Add(binding.rigidBody->Settings(), binding.collider->Settings(), binding.expectedPose, active);
				}
				else
				{
					working_->SetEnabled(binding.body, active);
				}
			}
			workingBodies_.swap(next);
		}

		std::span<const PhysicsWriteback> Simulate(const KT::World::World& world) override
		{
			accepting_ = false;
			// Scriptによる直接姿勢変更を、物理の書戻しで消してしまう前に拒否する。
			for (const auto& [entity, binding] : workingBodies_)
			{
				const auto& transform = world.GetComponent<LocalTransform>(entity);
				if (Pose{transform.position, transform.rotation} != binding.expectedPose || transform.scale != Vector3{1, 1, 1})
				{
					throw std::invalid_argument("ゲームcallbackから物理姿勢を直接編集できません。物理命令を使用してください。");
				}
			}
			for (const auto& [body, command] : commands_)
			{
				working_->Apply(body, command);
			}
			working_->Step();
			for (auto& [entity, binding] : workingBodies_)
			{
				const auto state = working_->GetState(binding.body);
				binding.expectedPose = state.pose;
				writebacks_.push_back({entity, state});
			}
			return writebacks_;
		}

		void Finish(const KT::World::World& world, const SceneUpdateContext& cpu) override
		{
			Synchronize(world, cpu);
			// 接触通知のEntity解決もcommit前に完了する。削除済み対象は通知しない。
			std::map<BodyHandle, Entity> entities;
			for (const auto& [entity, binding] : workingBodies_)
			{
				entities.emplace(binding.body, entity);
			}
			for (const auto& contact : working_->Contacts())
			{
				if (entities.contains(contact.first) && entities.contains(contact.second))
				{
					pendingEvents_.push_back(
						{contact.type, entities.at(contact.first), entities.at(contact.second), contact.normal, contact.sensor});
				}
			}
			working_->Seal();
		}

		void Commit() noexcept override
		{
			committed_.swap(working_);
			committedBodies_.swap(workingBodies_);
			events_.swap(pendingEvents_);
			accepting_ = false;
		}

		void Rollback() noexcept override
		{
			// 確定world・対応表・イベントには触らない。候補の解放も次Beginへ遅らせる。
			accepting_ = false;
		}

		// 成功済みworldは読取専用。失敗候補を保持してもこちらの状態は変えない。
		std::unique_ptr<PhysicsWorld> committed_;
		// 次tickの準備時に作り直し、commit時だけ確定側と交換する。
		std::unique_ptr<PhysicsWorld> working_;
		std::map<Entity, Binding, EntityLess> committedBodies_;
		std::map<Entity, Binding, EntityLess> workingBodies_;
		std::vector<std::pair<BodyHandle, BodyCommand>> commands_;
		std::vector<PhysicsWriteback> writebacks_;
		std::vector<PhysicsContact> events_;
		// 開始境界で削除・置換・無効化したEntityへの前tick通知を除いた配信値。
		std::vector<PhysicsContact> inputEvents_;
		std::vector<PhysicsContact> pendingEvents_;
		// Begin完了からSimulate開始まで、ゲーム側の予約・開始状態のqueryを許可する。
		bool accepting_ = false;
	};

	void PrepareScenePhysics(Scene& scene, PhysicsRuntime& runtime)
	{
		(void)scene.GetSnapshot();
		if (scene.HasPhysics())
		{
			return;
		}
		scene.AttachPhysics(std::make_unique<JoltScenePhysics>(scene, runtime));
	}
}
