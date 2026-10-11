#pragma once
#include <Core/Utility/NonCopyable.h>
#include <memory>

namespace KT::Physics
{
	class PhysicsWorld;

	// Joltのプロセス共通登録と単一threadの実行資源を所有する。
	// 複数Applicationが同時に存在しても、最後のWorld破棄まで登録を維持する。
	class PhysicsRuntime : private KT::Core::NonCopyable
	{
	public:
		PhysicsRuntime();
		~PhysicsRuntime();

	private:
		friend class PhysicsWorld;
		struct State;
		std::shared_ptr<State> state_;
	};
}
