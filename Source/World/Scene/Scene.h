#pragma once
#include <World/Scene/SceneUpdateContext.h>
#include <World/Scene/WorldCommandBuffer.h>
#include <World/Scene/ScriptComponent.h>
#include <array>

namespace KT::World
{
	// 一つのWorldと予約・CPU結果を所有する。Sceneの寿命とWorldの寿命をそろえる。
	// 全操作は単一スレッド。借用値を編集callbackの外へ保持しない。
	class Scene : private KT::Core::NonCopyable
	{
	public:
		// Script更新後に、同じCPU入力で呼ぶゲーム固有の更新処理。
		using GameUpdate = std::function<void(Scene&, const SceneUpdateContext&, double)>;
		explicit Scene(std::string name = {});
		~Scene();
		[[nodiscard]] const World& GetWorld() const;

		[[nodiscard]] const std::string& GetName() const noexcept
		{
			return name_;
		}

		WorldCommandBuffer& Commands();
		// 開始反映→CPU入力確定→Script・ゲーム更新→終了反映→CPU結果公開の順で行う。
		// 例外時は一回の更新全体を復元する。予約・更新番号・発行済み世代は消費する。
		const SceneUpdateContext& Update(double deltaSeconds, const GameUpdate& gameUpdate = {});
		[[nodiscard]] const SceneUpdateContext& GetSnapshot() const;

		[[nodiscard]] bool HasSnapshot() const noexcept
		{
			return state_ == State::Ready && snapshot_ != nullptr;
		}

		[[nodiscard]] bool IsUpdating() const noexcept
		{
			return state_ == State::Updating;
		}

		[[nodiscard]] bool HasFailed() const noexcept
		{
			return state_ == State::Failed;
		}

		// 今回の反映結果から、生存中の生成Entityを取得する。
		[[nodiscard]] Entity Resolve(DeferredEntity reservation) const;
		[[nodiscard]] std::optional<Entity> FindByUuid(ObjectUuid uuid) const;

		[[nodiscard]] const std::array<CommandFlushResult, 2>& GetCommandResults() const noexcept
		{
			return commandResults_;
		}

		// 復元後も再開は明示する。失敗原因を修正してから次Updateで再検証する。
		void ResetAfterFailure();
		void SetScriptEnabled(Entity entity, std::size_t scriptIndex, bool enabled);

		template <ComponentType T, class Fn> void EditComponent(Entity entity, Fn&& edit)
		{
			static_assert(!ReadOnlyComponent<T>, "Hierarchy・PersistentId・ScriptComponentの値編集には専用APIを使用してください。");

			// 編集可能な段階を確認し、待機中の完成結果を失効させる。
			RequireValueEditing();
			if (state_ == State::Ready)
			{
				snapshot_.reset();
			}

			// 編集callback中の更新・破棄を防ぎ、例外時も編集中の数を戻す。
			++valueEdits_;
			try
			{
				world_.BackupComponent<T>(entity);
				std::invoke(std::forward<Fn>(edit), world_.GetComponent<T>(entity));
				--valueEdits_;
			}
			catch (...)
			{
				--valueEdits_;
				throw;
			}
		}

		// ゲーム更新開始時の有効集合で列挙する。値編集は順次反映し、派生値は固定する。
		template <ComponentType... Ts, class Fn> void ForEachActive(Fn&& update)
		{
			static_assert(sizeof...(Ts) > 0);

			// 更新開始時に確定した有効状態と、現在のcomponent値を使う。
			if (!gamePhase_ || !gameInput_)
			{
				throw std::logic_error("ForEachActiveはSceneのゲーム更新処理中だけ使用できます。");
			}

			for (const auto& entity : gameInput_->GetTransforms())
			{
				if (entity.active.value && (world_.HasComponent<Ts>(entity.entity) && ...))
				{
					// callbackへ可変参照を渡す前に、対象の既存値を全て捕捉する。
					(BackupEditable<Ts>(entity.entity), ...);
					std::invoke(update, entity.entity, world_.GetComponent<Ts>(entity.entity)...);
				}
			}
		}

	private:
		friend class SceneLoader;
		friend class SceneHost;

		// 完成結果の公開可否と、失敗後の明示的な復旧待ちを表す。
		enum class State
		{
			Ready,
			Updating,
			Failed
		};
		void RequireValueEditing() const;
		template <ComponentType T> void BackupEditable(Entity entity)
		{
			if constexpr (!ReadOnlyComponent<T>)
			{
				world_.BackupComponent<T>(entity);
			}
		}
		std::unique_ptr<SceneUpdateContext> ComputeCpu(std::uint64_t updateNumber) const;

		// Entity・component・Script実体の唯一の所有先。
		World world_;
		std::string name_;
		// 構造変更を所有し、更新の開始・終了境界で順に反映する。
		WorldCommandBuffer commands_;
		// 成功したCPU結果。更新中と失敗後は隠し、rollback後の明示復旧で再公開する。
		std::unique_ptr<SceneUpdateContext> snapshot_;
		// 開始境界と終了境界の反映結果を、この順で保持する。
		std::array<CommandFlushResult, 2> commandResults_;
		// CPU更新の識別番号。失敗した更新でも番号は消費する。
		std::uint64_t updateNumber_ = 0;
		State state_ = State::Ready;
		// Script・ゲームcallback中だけ、値編集と有効Entityの列挙を許可する。
		bool gamePhase_ = false;
		// 実行中の値編集callback数。入れ子の編集も数える。
		std::size_t valueEdits_ = 0;
		// ゲーム更新中だけ借用する固定CPU入力。callbackの外へ保持しない。
		const SceneUpdateContext* gameInput_ = nullptr;
	};
}
