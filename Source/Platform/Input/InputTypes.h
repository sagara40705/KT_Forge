#pragma once
#include <cstddef>
#include <cstdint>

namespace KT::Platform
{
	// GLFWの定数とは独立した連続値。UnknownとCountは問合せに使わない。
	enum class Key : std::uint16_t
	{
		Unknown,
		Space,
		Apostrophe,
		Comma,
		Minus,
		Period,
		Slash,
		D0,
		D1,
		D2,
		D3,
		D4,
		D5,
		D6,
		D7,
		D8,
		D9,
		Semicolon,
		Equal,
		A,
		B,
		C,
		D,
		E,
		F,
		G,
		H,
		I,
		J,
		K,
		L,
		M,
		N,
		O,
		P,
		Q,
		R,
		S,
		T,
		U,
		V,
		W,
		X,
		Y,
		Z,
		LeftBracket,
		Backslash,
		RightBracket,
		GraveAccent,
		World1,
		World2,
		Escape,
		Enter,
		Tab,
		Backspace,
		Insert,
		Delete,
		Right,
		Left,
		Down,
		Up,
		PageUp,
		PageDown,
		Home,
		End,
		CapsLock,
		ScrollLock,
		NumLock,
		PrintScreen,
		Pause,
		F1,
		F2,
		F3,
		F4,
		F5,
		F6,
		F7,
		F8,
		F9,
		F10,
		F11,
		F12,
		F13,
		F14,
		F15,
		F16,
		F17,
		F18,
		F19,
		F20,
		F21,
		F22,
		F23,
		F24,
		F25,
		Keypad0,
		Keypad1,
		Keypad2,
		Keypad3,
		Keypad4,
		Keypad5,
		Keypad6,
		Keypad7,
		Keypad8,
		Keypad9,
		KeypadDecimal,
		KeypadDivide,
		KeypadMultiply,
		KeypadSubtract,
		KeypadAdd,
		KeypadEnter,
		KeypadEqual,
		LeftShift,
		LeftControl,
		LeftAlt,
		LeftSuper,
		RightShift,
		RightControl,
		RightAlt,
		RightSuper,
		Menu,
		Count
	};

	enum class MouseButton : std::uint8_t
	{
		Left,
		Right,
		Middle,
		Button4,
		Button5,
		Button6,
		Button7,
		Button8,
		Count
	};

	enum class GamepadButton : std::uint8_t
	{
		A,
		B,
		X,
		Y,
		LeftBumper,
		RightBumper,
		Back,
		Start,
		Guide,
		LeftThumb,
		RightThumb,
		DpadUp,
		DpadRight,
		DpadDown,
		DpadLeft,
		Count
	};

	enum class GamepadStatus : std::uint8_t
	{
		Disconnected,
		Unmapped,
		Ready,
		ReadFailed
	};

	enum class CursorMode : std::uint8_t
	{
		Normal,
		// 非表示にして無制限の相対移動を取得する。GLFW_CURSOR_DISABLEDへ対応。
		Captured
	};

	inline constexpr std::size_t KeyCount = static_cast<std::size_t>(Key::Count);
	inline constexpr std::size_t MouseButtonCount = static_cast<std::size_t>(MouseButton::Count);
	inline constexpr std::size_t GamepadButtonCount = static_cast<std::size_t>(GamepadButton::Count);
	// 実行時のスロットであり、恒久的な機器ID/プレイヤーIDではない。
	inline constexpr std::size_t GamepadSlotCount = 16;

	struct ButtonState
	{
		bool down = false;
		bool pressed = false;
		bool released = false;
	};
}
