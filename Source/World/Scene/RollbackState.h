#pragma once
#include <memory>
#include <type_traits>
#include <utility>

namespace KT::World
{
	// 実行時の復元値を所有する。Restoreは確保・例外・外部副作用を伴わない。
	// 捕捉は対象を変更しない。外部参照先やファイル等はSceneの復元対象に含めない。
	// 捕捉・復元・記録破棄中はScene/Worldへ再入しない。
	class RollbackState
	{
	public:
		virtual ~RollbackState() noexcept = default;
		virtual void Restore() noexcept = 0;
	};

	// 内部の可変状態を持たないScriptが、復元不要の契約を明示する。
	class StatelessRollback final : public RollbackState
	{
	public:
		void Restore() noexcept override
		{
		}
	};

	// コピーで独立した状態を得られる値を保持し、同じ実体へ例外なしで戻す。
	// ポインター等の参照先の状態を持つ型は、独自のRollbackStateを実装する。
	template <class T> class ValueRollback final : public RollbackState
	{
		static_assert(std::is_copy_constructible_v<T> && std::is_nothrow_swappable_v<T>,
			"値のrollbackには独立したコピーと例外を送出しないswapが必要です。");

	public:
		explicit ValueRollback(T& value)
			: value_(value), saved_(value)
		{
		}

		void Restore() noexcept override
		{
			using std::swap;
			swap(value_, saved_);
		}

	private:
		// 構造変更で削除されても、更新完了まではWorldが実体を保持する。
		T& value_;
		// 捕捉時点の独立した値。Scene保存の設定とは別に保持する。
		T saved_;
	};
}
