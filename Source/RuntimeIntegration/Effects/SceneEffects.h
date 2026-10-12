#pragma once
#include <Renderer/Effects/EffectPlayer.h>
#include <World/Entity.h>
#include <World/Scene/SceneUpdateContext.h>
#include <optional>
#include <vector>

namespace KT::RuntimeIntegration
{
	// Playerと世代を識別するゲーム用handle。停止・終了後は再利用しない。
	struct EffectHandle
	{
		std::uint64_t owner = 0, id = 0;
		bool operator==(const EffectHandle&) const = default;
	};

	// ゲーム操作をCPU更新成功まで保留し、完成World行列を描画側へコピーする。
	// Playerを借用する。ゲームcallbackと同じthreadで使い、callback外へComponent参照を保持しない。
	class SceneEffects : private KT::Core::NonCopyable
	{
	public:
		explicit SceneEffects(KT::Renderer::EffectPlayer& player);
		// 起動時の同期読込。素材と相対依存はPlayer終了まで保持する。
		KT::Renderer::EffectAsset Load(const std::filesystem::path& path);
		// 固定World行列で再生する。変更は次の成功tickに反映する。
		EffectHandle Play(KT::Renderer::EffectAsset asset, const KT::Core::Math::Matrix4& world = {});
		// 追従先の完成Worldへlocalを乗じる。削除・別World・inactiveでは停止する。
		EffectHandle PlayAttached(KT::Renderer::EffectAsset asset, KT::World::Entity target, const KT::Core::Math::Matrix4& local = {});
		void Follow(EffectHandle handle, KT::World::Entity target, const KT::Core::Math::Matrix4& local = {});
		// 固定位置へ切り替え、追従を解除する。TRSで位置・回転・拡縮を指定できる。
		void SetTransform(EffectHandle handle, const KT::Core::Math::Matrix4& world);
		void Stop(EffectHandle handle);
		void StopAll();
		[[nodiscard]] bool IsPlaying(EffectHandle handle) const;
		[[nodiscard]] std::size_t PlayingCount() const;

		// Applicationのtick境界。失敗時は再生予約だけを復元し、上流Managerを変更しない。
		void BeginTick();
		void AbortTick() noexcept;
		void CompleteTick(const KT::World::SceneUpdateContext& cpu, double deltaSeconds);

	private:
		// Runtimeの再生値と追従先。永続Sceneへ再生途中のhandleを保存しない。
		struct Entry
		{
			KT::Renderer::EffectInstance instance;
			std::optional<KT::World::Entity> target;
			KT::Core::Math::Matrix4 local;
			bool started = false;
		};

		Entry& Require(EffectHandle handle);
		KT::Renderer::EffectPlayer& player_;
		// 予約と前tickの値。失敗時にはswapで確保せず戻す。
		std::vector<Entry> entries_;
		std::vector<Entry> checkpoint_;
		std::uint64_t nextInstance_ = 1;
		std::uint64_t worldId_ = 0;
		bool ticking_ = false;
	};
}
