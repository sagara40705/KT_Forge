#include <RuntimeIntegration/Effects/SceneEffects.h>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace KT::RuntimeIntegration
{
	SceneEffects::SceneEffects(KT::Renderer::EffectPlayer& player)
		: player_(player)
	{
	}

	KT::Renderer::EffectAsset SceneEffects::Load(const std::filesystem::path& path)
	{
		if (ticking_)
		{
			throw std::logic_error("Effect素材はゲームtickの前に読み込んでください。");
		}
		return player_.Load(path);
	}

	EffectHandle SceneEffects::Play(KT::Renderer::EffectAsset asset, const KT::Core::Math::Matrix4& world)
	{
		if (!player_.Contains(asset))
		{
			throw std::invalid_argument("Effect再生には同じPlayerで読み込んだ素材が必要です。");
		}
		if (!KT::Renderer::IsValidEffectTransform(world))
		{
			throw std::invalid_argument("EffectのWorld行列は有限affineで、拡縮のゼロ・過小・過大を避けてください。");
		}
		if (entries_.size() >= 256 || nextInstance_ == (std::numeric_limits<std::uint64_t>::max)())
		{
			throw std::overflow_error("Effect再生数またはhandle番号が上限に達しました。");
		}
		const auto id = nextInstance_++;
		entries_.push_back({{id, asset, world}, {}, {}, false});
		return {player_.OwnerId(), id};
	}

	EffectHandle SceneEffects::PlayAttached(KT::Renderer::EffectAsset asset, KT::World::Entity target, const KT::Core::Math::Matrix4& local)
	{
		if (!target.IsValid() || !KT::Renderer::IsValidEffectTransform(local))
		{
			throw std::invalid_argument("Effect追従には有効なEntityと、有限affineで分解可能なlocal行列が必要です。");
		}
		const auto handle = Play(asset);
		auto& entry = entries_.back();
		entry.target = target;
		entry.local = local;
		return handle;
	}

	SceneEffects::Entry& SceneEffects::Require(EffectHandle handle)
	{
		if (handle.owner != player_.OwnerId() || handle.id == 0)
		{
			throw std::invalid_argument("Effect handleはこのPlayerに属していません。");
		}
		const auto found =
			std::find_if(entries_.begin(), entries_.end(), [handle](const auto& entry) { return entry.instance.id == handle.id; });
		if (found == entries_.end() || (found->started && !player_.IsPlaying(handle.id)))
		{
			throw std::invalid_argument("Effect handleの再生は停止または終了しています。");
		}
		return *found;
	}

	void SceneEffects::Follow(EffectHandle handle, KT::World::Entity target, const KT::Core::Math::Matrix4& local)
	{
		if (!target.IsValid() || !KT::Renderer::IsValidEffectTransform(local))
		{
			throw std::invalid_argument("Effect追従先のEntityまたはlocal行列が不正です。");
		}
		auto& entry = Require(handle);
		entry.target = target;
		entry.local = local;
	}

	void SceneEffects::SetTransform(EffectHandle handle, const KT::Core::Math::Matrix4& world)
	{
		if (!KT::Renderer::IsValidEffectTransform(world))
		{
			throw std::invalid_argument("EffectのWorld行列は有限affineで、拡縮のゼロ・過小・過大を避けてください。");
		}
		auto& entry = Require(handle);
		entry.instance.world = world;
		entry.target.reset();
	}

	void SceneEffects::Stop(EffectHandle handle)
	{
		// 停止済みhandleは何もしない。別Playerのhandleは拒否する。
		if (handle.owner != player_.OwnerId() || handle.id == 0)
		{
			throw std::invalid_argument("停止するEffect handleはこのPlayerに属していません。");
		}
		std::erase_if(entries_, [handle](const auto& entry) { return entry.instance.id == handle.id; });
	}

	void SceneEffects::StopAll()
	{
		entries_.clear();
	}

	bool SceneEffects::IsPlaying(EffectHandle handle) const
	{
		if (handle.owner != player_.OwnerId())
		{
			return false;
		}
		for (const auto& entry : entries_)
		{
			if (entry.instance.id == handle.id)
			{
				return !entry.started || player_.IsPlaying(handle.id);
			}
		}
		return false;
	}

	std::size_t SceneEffects::PlayingCount() const
	{
		std::size_t count = 0;
		for (const auto& entry : entries_)
		{
			count += !entry.started || player_.IsPlaying(entry.instance.id) ? 1 : 0;
		}
		return count;
	}

	void SceneEffects::BeginTick()
	{
		if (ticking_)
		{
			throw std::logic_error("Effect tickへ再入できません。");
		}
		checkpoint_ = entries_;
		ticking_ = true;
	}

	void SceneEffects::AbortTick() noexcept
	{
		if (ticking_)
		{
			entries_.swap(checkpoint_);
			checkpoint_.clear();
			ticking_ = false;
		}
	}

	void SceneEffects::CompleteTick(const KT::World::SceneUpdateContext& cpu, double deltaSeconds)
	{
		// Scene切替では旧個体を捨て、新Sceneのcallbackで発行した予約だけを残す。
		auto pending = entries_;
		const auto& transforms = cpu.GetTransforms();
		std::erase_if(pending,
			[&](auto& entry)
			{
				if (entry.started && (worldId_ != cpu.SourceWorldId() || !player_.IsPlaying(entry.instance.id)))
				{
					return true;
				}
				if (entry.target)
				{
					const auto found = std::find_if(
						transforms.begin(), transforms.end(), [&](const auto& entity) { return entity.entity == *entry.target; });
					if (found == transforms.end() || !found->active.value)
					{
						return true;
					}
					entry.instance.world = entry.local * found->transform.matrix;
				}
				return false;
			});
		std::vector<KT::Renderer::EffectInstance> instances;
		instances.reserve(pending.size());
		for (const auto& entry : pending)
		{
			instances.push_back(entry.instance);
		}
		player_.Update(instances, deltaSeconds);
		for (auto& entry : pending)
		{
			entry.started = true;
		}
		entries_.swap(pending);
		worldId_ = cpu.SourceWorldId();
		checkpoint_.clear();
		ticking_ = false;
	}
}
