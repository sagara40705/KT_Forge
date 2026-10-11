#include <Physics/JoltWorldInternal.h>

namespace KT::Physics
{
	void PhysicsWorld::Seal()
	{
		impl_->RequireMutable();

		// 削除済み・無効化済みBodyの古いcontact cacheを次のworldへ持ち込まない。
		class Filter final : public JPH::StateRecorderFilter
		{
		public:
			const Impl& world;

			explicit Filter(const Impl& input)
				: world(input)
			{
			}

			bool ShouldSaveContact(const JPH::BodyID& first, const JPH::BodyID& second) const override
			{
				const auto valid = [&](JPH::BodyID id)
				{
					const auto packed = id.GetIndexAndSequenceNumber();
					const auto handle = world.handles.find(packed);
					return handle != world.handles.end() && !world.retired.contains(packed) && world.Find(handle->second).enabled;
				};
				return valid(first) && valid(second);
			}
		};

		Filter filter(*impl_);
		CheckpointStream checkpoint;
		impl_->system.SaveState(checkpoint, JPH::EStateRecorderState::All, &filter);
		if (checkpoint.failed)
		{
			throw std::runtime_error("物理checkpointの保存領域を確保できませんでした。");
		}
		impl_->checkpoint.bytes.swap(checkpoint.bytes);
		impl_->sealed = true;
	}

	std::unique_ptr<PhysicsWorld> PhysicsWorld::Clone() const
	{
		if (!impl_->sealed)
		{
			throw std::logic_error("物理worldを複製する前にSealを完了してください。");
		}
		auto candidate = std::make_unique<Impl>(impl_->runtime, impl_->settings);
		candidate->nextHandle = impl_->nextHandle;
		candidate->bodySlots = impl_->bodySlots;
		auto& interface = candidate->system.GetBodyInterface();
		for (const auto& [handle, record] : impl_->bodies)
		{
			// 生成設定・形状とBodyIDを一致させてからsimulation stateを復元する。
			auto creation = record.creation;
			creation.mPosition = record.body->GetPosition();
			creation.mRotation = record.body->GetRotation();
			auto [entry, inserted] = candidate->bodies.emplace(handle, Impl::Record{nullptr, creation, record.motion, false});
			candidate->handles.emplace(record.body->GetID().GetIndexAndSequenceNumber(), handle);
			auto* body = interface.CreateBodyWithID(record.body->GetID(), creation);
			if (!body)
			{
				candidate->bodies.erase(entry);
				throw std::runtime_error("物理checkpointと同じBodyIDを構築できませんでした。");
			}
			entry->second.body = body;

			// broadphase外の無効Bodyも、速度・sleep用内部状態を保持する。
			CheckpointStream bodyState;
			record.body->SaveState(bodyState);
			if (bodyState.failed)
			{
				throw std::runtime_error("無効Bodyを含む物理状態を複製できませんでした。");
			}
			body->RestoreState(bodyState);
			if (record.enabled)
			{
				interface.AddBody(body->GetID(), JPH::EActivation::DontActivate);
				entry->second.enabled = true;
			}
		}

		// 読取cursorは候補側へコピーする。確定worldにはSaveStateもRestoreStateも呼ばない。
		CheckpointStream checkpoint;
		checkpoint.bytes = impl_->checkpoint.bytes;
		if (!candidate->system.RestoreState(checkpoint) || checkpoint.failed || !checkpoint.IsEOF())
		{
			throw std::runtime_error("物理checkpointの復元に失敗しました。");
		}
		candidate->listener.entries.clear();
		candidate->listener.failed = false;
		return std::unique_ptr<PhysicsWorld>(new PhysicsWorld(std::move(candidate)));
	}
}
