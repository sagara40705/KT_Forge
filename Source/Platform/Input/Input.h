#pragma once
#include "InputSnapshot.h"
#include <Core/Utility/NonCopyable.h>

namespace KT::Platform
{
	class Window;

	// AIによる仮実装。Windowを借用し、Windowより先に破棄する。
	// 全操作はメインスレッド。1 Windowにつき1 Input、copy/move禁止。
	class Input : private KT::Core::NonCopyable
	{
	public:
		explicit Input(Window& window);
		~Input();

		// 呼出側がPollEventsを済ませてから、CPU更新ごとに1回呼ぶ。
		// イベント処理/待機はしない。短いキー/マウス入力の両edgeを保持する。
		void Update();
		const InputSnapshot& GetSnapshot() const noexcept;
		void SetCursorMode(CursorMode mode);
		// finiteかつ[0,1)。左右スティック共通、次Updateから適用する。
		void SetStickDeadZone(float deadZone);

	private:
		friend class Window;
		void OnKey(int key, int action) noexcept;
		void OnMouseButton(int button, int action) noexcept;
		void OnCursorPosition(double x, double y) noexcept;
		void OnScroll(double x, double y) noexcept;
		void OnFocus(bool focused) noexcept;
		void ResetCursorBaseline() noexcept;
		void SampleKeyboardAndMouseBaseline() noexcept;
		void SampleGamepads(InputSnapshot& next) noexcept;

		Window& window_;
		InputSnapshot snapshot_{};
		// callbackは公開snapshotを変更せず、未公開状態へ蓄積する。
		KeyboardState pendingKeyboard_{};
		MouseState pendingMouse_{};
		bool focused_ = false;
		bool cursorBaselineValid_ = false;
		CursorMode cursorMode_ = CursorMode::Normal;
		float stickDeadZone_ = 0.15f;
		std::array<bool, GamepadSlotCount> gamepadNeedsBaseline_{};
		std::array<std::array<bool, GamepadButtonCount>, GamepadSlotCount> gamepadReleasePending_{};
	};
}
