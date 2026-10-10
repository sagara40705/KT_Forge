#include <World/Scene/WorldCommandBuffer.h>
#include <algorithm>
#include <atomic>

namespace KT::World
{
	namespace
	{
		// 発行元IDを再利用せず、最大値は発行停止のために残す。
		std::atomic<std::uint64_t> nextBufferId{1};

		std::uint64_t AcquireBufferId()
		{
			auto candidate = nextBufferId.load(std::memory_order_relaxed);
			for (;;)
			{
				if (candidate == (std::numeric_limits<std::uint64_t>::max)())
				{
					throw std::overflow_error("WorldCommandBufferの識別IDが上限に達しました。");
				}
				if (nextBufferId.compare_exchange_weak(candidate, candidate + 1, std::memory_order_relaxed))
				{
					return candidate;
				}
			}
		}
	}

	Entity CommandFlushResult::Resolve(DeferredEntity reservation) const
	{
		for (const auto& entry : created)
		{
			if (entry.reservation == reservation)
			{
				return entry.entity;
			}
		}
		throw std::invalid_argument("指定した生成予約はこの反映結果に含まれていません。");
	}

	// 永続UUIDと表示名を所有し、反映時にScene用Entityを生成する。
	struct WorldCommandBuffer::CreateCommand final : Command
	{
		DeferredEntity reservation;
		ObjectUuid uuid;
		std::string name;

		CreateCommand(DeferredEntity token, ObjectUuid identity, std::string label)
			: reservation(token),
			  uuid(identity),
			  name(std::move(label))
		{
		}

		bool Apply(WorldCommandBuffer& buffer, ApplyContext& context) override
		{
			// 結果表はFlushで確保済み。作成成功後に記録で失敗しない。
			const auto entity = buffer.world_.CreateSceneEntity(uuid, std::move(name));
			context.result.created.push_back({reservation, entity});
			return true;
		}
	};

	// 部分木を削除し、同じ反映内で完了した削除だけを省略する。
	struct WorldCommandBuffer::DestroyCommand final : Command
	{
		EntityTarget target;

		explicit DestroyCommand(EntityTarget entity)
			: target(entity)
		{
		}

		bool Apply(WorldCommandBuffer& buffer, ApplyContext& context) override
		{
			const auto entity = buffer.ResolveTarget(target, context);

			// 生存確認はWorld側で行い、同じ世代の重複削除だけを先に除く。
			if (std::find(context.destroyed.begin(), context.destroyed.end(), entity) != context.destroyed.end())
			{
				return false;
			}

			buffer.world_.DestroyEntity(entity, context.destroyed);
			return true;
		}
	};

	// 親子双方の生成予約を解決し、Worldの階層検証を通して親を変更する。
	struct WorldCommandBuffer::ParentCommand final : Command
	{
		EntityTarget child;
		EntityTarget parent;
		// 行列は保存せず、反映時点で指定した維持方法を適用する。
		ParentChangeMode mode;

		ParentCommand(EntityTarget childTarget, EntityTarget parentTarget, ParentChangeMode changeMode)
			: child(childTarget),
			  parent(parentTarget),
			  mode(changeMode)
		{
		}

		bool Apply(WorldCommandBuffer& buffer, ApplyContext& context) override
		{
			const auto childEntity = buffer.ResolveTarget(child, context);
			const auto parentEntity = buffer.ResolveTarget(parent, context, true);
			buffer.world_.SetParent(childEntity, parentEntity, mode);
			return true;
		}
	};

	WorldCommandBuffer::WorldCommandBuffer(World& world)
		: world_(world),
		  bufferId_(AcquireBufferId())
	{
	}

	void WorldCommandBuffer::RequireAccepting() const
	{
		if (!accepting_ || flushing_)
		{
			throw std::logic_error("予約の反映・破棄中、または受付停止中はWorldCommandBufferへコマンドを追加できません。");
		}
	}

	void WorldCommandBuffer::ValidateTarget(EntityTarget target, bool allowRoot) const
	{
		// 生成済みの対象は、記録時点で同じWorldの生存Entityであることを確認する。
		if (const auto* entity = std::get_if<Entity>(&target.value))
		{
			if (allowRoot && *entity == Entity{})
			{
				return;
			}
			if (!world_.IsAlive(*entity))
			{
				throw std::invalid_argument("予約コマンドの対象Entityが無効・失効済み、または別Worldのものです。");
			}
			return;
		}

		// 未生成の対象は、現在の予約列内の生成コマンドだけを受け付ける。
		const auto token = std::get<DeferredEntity>(target.value);
		if (token.bufferId != bufferId_ || token.batch != batch_ || token.commandIndex >= commands_.size() ||
			!dynamic_cast<const CreateCommand*>(commands_[token.commandIndex].get()))
		{
			throw std::invalid_argument("生成予約が別のWorldCommandBuffer・反映単位のもの、または有効な生成コマンドを指していません。");
		}
	}

	Entity WorldCommandBuffer::ResolveTarget(EntityTarget target, const ApplyContext& context, bool allowRoot) const
	{
		if (const auto* entity = std::get_if<Entity>(&target.value))
		{
			if (allowRoot && *entity == Entity{})
			{
				return {};
			}
			return *entity;
		}
		return context.result.Resolve(std::get<DeferredEntity>(target.value));
	}

	DeferredEntity WorldCommandBuffer::CreateEntity(std::string name, ObjectUuid uuid)
	{
		RequireAccepting();

		// UUIDの指定がない場合は、予約時に保存用の識別値を発行する。
		if (!uuid.IsValid())
		{
			uuid = ObjectUuid::Generate();
		}

		const DeferredEntity token{bufferId_, batch_, commands_.size()};
		commands_.push_back(std::make_unique<CreateCommand>(token, uuid, std::move(name)));
		return token;
	}

	void WorldCommandBuffer::DestroyEntity(EntityTarget entity)
	{
		RequireAccepting();
		ValidateTarget(entity);

		commands_.push_back(std::make_unique<DestroyCommand>(entity));
	}

	void WorldCommandBuffer::SetParent(EntityTarget child, EntityTarget parent, ParentChangeMode mode)
	{
		RequireAccepting();
		ValidateTarget(child);
		ValidateTarget(parent, true);
		if (mode != ParentChangeMode::KeepLocal && mode != ParentChangeMode::KeepWorld)
		{
			throw std::invalid_argument("親変更の変換維持方法が不正です。");
		}

		commands_.push_back(std::make_unique<ParentCommand>(child, parent, mode));
	}

	CommandFlushResult WorldCommandBuffer::Flush()
	{
		RequireAccepting();
		world_.RequireStructuralChange();
		if (batch_ == (std::numeric_limits<std::uint64_t>::max)())
		{
			throw std::overflow_error("WorldCommandBufferの反映単位の番号が上限に達しました。");
		}

		// 記録列を固定し、反映中の追加と再入を禁止する。
		flushing_ = true;
		ApplyContext context;
		std::size_t index = 0;
		try
		{
			// 生成結果の記録領域を先に確保し、予約を記録順に反映する。
			context.result.created.reserve(commands_.size());
			for (; index < commands_.size(); ++index)
			{
				if (commands_[index]->Apply(*this, context))
				{
					++context.result.applied;
				}
				else
				{
					++context.result.skipped;
				}
			}
		}
		catch (...)
		{
			// 適用済みの変更を保持し、最初の失敗位置と例外を記録する。
			context.result.failedCommand = index;
			context.result.error = std::current_exception();
			accepting_ = false;
		}

		// 失敗した操作と未実行操作も廃棄し、自動で再試行しない。
		commands_.clear();
		++batch_;
		flushing_ = false;
		return std::move(context.result);
	}

	void WorldCommandBuffer::Discard() noexcept
	{
		// 所有引数の破棄中も予約追加を拒否する。
		flushing_ = true;
		commands_.clear();
		if (batch_ != (std::numeric_limits<std::uint64_t>::max)())
		{
			++batch_;
		}
		flushing_ = false;
	}
}
