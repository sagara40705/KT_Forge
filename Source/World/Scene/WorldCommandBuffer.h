#pragma once
#include <World/World.h>
#include <exception>
#include <limits>
#include <optional>
#include <tuple>
#include <variant>

namespace KT::World
{
	// 所有値として記録できない参照ラッパーを、テンプレート引数から検出する。
	template <class T> inline constexpr bool IsReferenceArgument = false;
	template <class T> inline constexpr bool IsReferenceArgument<std::reference_wrapper<T>> = true;

	// 未生成の対象。同じbuffer・反映batch内だけで予約の対象にできる。
	struct DeferredEntity
	{
		// 別の予約バッファーで発行した対象との混同を防ぐ識別値。
		std::uint64_t bufferId = 0;
		// 一度の反映・破棄で失効する、予約列の識別番号。
		std::uint64_t batch = 0;
		// 同じ予約列内で、この対象を生成するコマンドの位置。
		std::size_t commandIndex = 0;
		bool operator==(const DeferredEntity&) const = default;
	};

	// 生存Entityまたは未反映の生成予約を、同じ編集窓口へ渡す対象値。
	struct EntityTarget
	{
		// 既定の空Entityは、親指定時にだけルートを表す。
		std::variant<Entity, DeferredEntity> value{Entity{}};
		EntityTarget() = default;

		EntityTarget(Entity entity)
			: value(entity)
		{
		}

		EntityTarget(DeferredEntity entity)
			: value(entity)
		{
		}
	};

	// 生成予約と、反映時に発行したEntityの対応を保持する。
	struct CreatedEntity
	{
		DeferredEntity reservation;
		Entity entity;
	};

	// 境界ごとの実行診断。変更の確定はSceneの更新全体が成功したときに行う。
	struct CommandFlushResult
	{
		// この反映で生成できたEntity。後続の削除で失効する場合がある。
		std::vector<CreatedEntity> created;
		// 状態変更まで実行したコマンド数。rolledBack時は変更が残っていない。
		std::size_t applied = 0;
		// 同じ反映内ですでに破棄したEntityへの重複削除数。
		std::size_t skipped = 0;
		// 失敗したコマンド位置。事前確保の失敗は先頭位置として記録する。
		std::optional<std::size_t> failedCommand;
		// 反映の例外。rolledBack時は、この反映またはScene更新全体の失敗原因を保持する。
		std::exception_ptr error;
		// Scene更新の失敗で、この境界の変更も全て戻したことを表す。
		bool rolledBack = false;

		[[nodiscard]] bool Succeeded() const noexcept
		{
			return !error && !rolledBack;
		}

		[[nodiscard]] Entity Resolve(DeferredEntity reservation) const;
	};

	// Worldを借用し、設定値を所有する。反映はSceneの更新境界だけで行う。
	// 引数の値には外部参照を埋め込まない。constructor/destructorから再入しない。
	class WorldCommandBuffer : private KT::Core::NonCopyable
	{
	private:
		// 一度の反映結果と、重複削除を判定する破棄済み集合を保持する。
		struct ApplyContext
		{
			CommandFlushResult result;
			// スロットが再利用されても区別できるよう、Entityの世代も保持する。
			std::vector<Entity> destroyed;
		};

		// 構造変更を記録順に適用する内部窓口。falseは重複削除の省略を表す。
		struct Command
		{
			virtual ~Command() = default;
			virtual bool Apply(WorldCommandBuffer& buffer, ApplyContext& context) = 0;
		};
		struct CreateCommand;
		struct DestroyCommand;
		struct ParentCommand;

		// componentの構築引数を所有し、反映時に一度だけ移動する。
		template <ComponentType T, class... Args> struct AddCommand final : Command
		{
			EntityTarget target;
			// 外部の引数の寿命に依存しない、値として記録した構築引数。
			std::tuple<Args...> arguments;

			AddCommand(EntityTarget entity, Args&&... values)
				: target(entity),
				  arguments(std::move(values)...)
			{
			}

			bool Apply(WorldCommandBuffer& buffer, ApplyContext& context) override
			{
				// 生成予約を解決してから、所有している値でcomponentを構築する。
				const auto entity = buffer.ResolveTarget(target, context);
				std::apply([&](auto&... values) { buffer.world_.AddComponentInternal<T>(entity, false, std::move(values)...); }, arguments);
				return true;
			}
		};

		// 対象のcomponentを反映時に削除する。型がなければ反映を失敗させる。
		template <ComponentType T> struct RemoveCommand final : Command
		{
			EntityTarget target;

			explicit RemoveCommand(EntityTarget entity)
				: target(entity)
			{
			}

			bool Apply(WorldCommandBuffer& buffer, ApplyContext& context) override
			{
				buffer.world_.RemoveComponent<T>(buffer.ResolveTarget(target, context));
				return true;
			}
		};

	public:
		[[nodiscard]] DeferredEntity CreateEntity(std::string name = {}, ObjectUuid uuid = {});
		void DestroyEntity(EntityTarget entity);
		// KeepWorldは記録時のsnapshotではなく、先行予約を反映した時点のWorld行列を維持する。
		void SetParent(EntityTarget child, EntityTarget parent = {}, ParentChangeMode mode = ParentChangeMode::KeepLocal);

		template <ComponentType T, class... Args> void AddComponent(EntityTarget entity, Args&&... arguments)
		{
			static_assert(!ReservedComponent<T>, "HierarchyとPersistentIdの追加・削除には専用APIを使用してください。");
			static_assert((!std::is_pointer_v<std::decay_t<Args>> && ...),
				"予約コマンドの引数は値を所有する必要があります。生ポインターは指定できません。");
			static_assert(
				(!IsReferenceArgument<std::decay_t<Args>> && ...), "予約コマンドの引数にstd::reference_wrapperは指定できません。");

			// 受付状態と対象を検査し、引数を所有するコマンドとして記録する。
			RequireAccepting();
			ValidateTarget(entity);

			commands_.push_back(
				std::make_unique<AddCommand<T, std::decay_t<Args>...>>(entity, std::decay_t<Args>(std::forward<Args>(arguments))...));
		}

		template <ComponentType T> void RemoveComponent(EntityTarget entity)
		{
			static_assert(!std::is_same_v<T, Name>, "SceneオブジェクトのNameは値だけを編集し、削除しません。");
			static_assert(!ReservedComponent<T>, "HierarchyとPersistentIdの追加・削除には専用APIを使用してください。");

			// 受付状態と対象を検査してから、削除を予約する。
			RequireAccepting();
			ValidateTarget(entity);

			commands_.push_back(std::make_unique<RemoveCommand<T>>(entity));
		}

		[[nodiscard]] std::size_t PendingCount() const noexcept
		{
			return commands_.size();
		}

	private:
		friend class Scene;
		friend class SceneLoader;
		explicit WorldCommandBuffer(World& world);
		CommandFlushResult Flush();
		void Discard() noexcept;
		void RequireAccepting() const;
		void ValidateTarget(EntityTarget target, bool allowRoot = false) const;
		Entity ResolveTarget(EntityTarget target, const ApplyContext& context, bool allowRoot = false) const;

		// Sceneが所有する反映先。予約バッファーより長く生存する。
		World& world_;
		// プロセス内で再発行しない、生成予約の発行元ID。
		std::uint64_t bufferId_;
		// 反映・破棄のたびに進め、以前の生成予約を失効させる。
		std::uint64_t batch_ = 1;
		// 記録順の構造変更と、その所有引数。
		std::vector<std::unique_ptr<Command>> commands_;
		// 反映・引数破棄中の追加と再入を禁止する。
		bool flushing_ = false;
		// 失敗後とScene破棄時は受付を止め、明示的な復旧まで再開しない。
		bool accepting_ = true;
	};
}
