#pragma once
#include <Physics/PhysicsTypes.h>

namespace KT::World
{
	class Scene;

	// 設定の変更はcomponentの置換で行う。計算後の状態はSceneだけが書き戻す。
	class RigidBody
	{
	public:
		explicit RigidBody(KT::Physics::BodySettings settings = {})
			: settings_(settings)
		{
			KT::Physics::Validate(settings_);
		}

		[[nodiscard]] const KT::Physics::BodySettings& Settings() const noexcept
		{
			return settings_;
		}

		[[nodiscard]] const KT::Physics::BodyState& State() const noexcept
		{
			return state_;
		}

	private:
		friend class Scene;
		KT::Physics::BodySettings settings_;
		// 保存しない実行時結果。物理未準備時には初期値を返す。
		KT::Physics::BodyState state_;
	};

	// Entityに一つだけ付ける衝突形状。非一様scaleは描画用子Entityへ置く。
	class Collider
	{
	public:
		explicit Collider(KT::Physics::ColliderSettings settings = {})
			: settings_(settings)
		{
			KT::Physics::Validate(settings_);
		}

		[[nodiscard]] const KT::Physics::ColliderSettings& Settings() const noexcept
		{
			return settings_;
		}

	private:
		KT::Physics::ColliderSettings settings_;
	};
}
