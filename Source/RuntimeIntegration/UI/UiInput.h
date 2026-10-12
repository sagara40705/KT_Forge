#pragma once
#include <Platform/Input/InputSnapshot.h>

namespace Rml
{
	class Context;
}

namespace KT::RuntimeIntegration
{
	// UIへ入力を先に送り、消費した入力と閉じた後の押しっぱなしをゲームから除く。
	// 0tickの描画ではゲームedgeを保持し、最初の成功tickだけで消費する。
	class UiInput
	{
	public:
		void Route(Rml::Context& context, const KT::Platform::InputSnapshot& raw, int windowWidth, int windowHeight, bool modal);
		void Consume() noexcept;
		[[nodiscard]] const KT::Platform::InputSnapshot& GameInput() const noexcept;

	private:
		// 最初の成功simulation tickまで、未消費のedgeとmouse差分を蓄積する。
		KT::Platform::InputSnapshot pending_;
		// UIで押した操作は、画面を閉じても物理releaseまで遮断する。
		std::array<bool, KT::Platform::KeyCount> blockedKeys_{};
		std::array<bool, KT::Platform::MouseButtonCount> blockedMouse_{};

		// 操作ごとにneutralを待つ。未対応triggerの0.5でpad全体を遮断し続けない。
		struct PadCapture
		{
			std::array<bool, KT::Platform::GamepadButtonCount> buttons{};
			bool leftStick = false, rightStick = false, leftTrigger = false, rightTrigger = false;
		};

		std::array<PadCapture, KT::Platform::GamepadSlotCount> blockedPads_{};
	};
}
