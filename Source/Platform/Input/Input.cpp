#include "Input.h"
#include <Platform/Window.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
	using namespace KT::Platform;

	struct KeyMapping
	{
		Key key;
		int nativeKey;
	};

	constexpr auto keyMappings = std::to_array<KeyMapping>({
		{Key::Space, GLFW_KEY_SPACE},
		{Key::Apostrophe, GLFW_KEY_APOSTROPHE},
		{Key::Comma, GLFW_KEY_COMMA},
		{Key::Minus, GLFW_KEY_MINUS},
		{Key::Period, GLFW_KEY_PERIOD},
		{Key::Slash, GLFW_KEY_SLASH},
		{Key::D0, GLFW_KEY_0},
		{Key::D1, GLFW_KEY_1},
		{Key::D2, GLFW_KEY_2},
		{Key::D3, GLFW_KEY_3},
		{Key::D4, GLFW_KEY_4},
		{Key::D5, GLFW_KEY_5},
		{Key::D6, GLFW_KEY_6},
		{Key::D7, GLFW_KEY_7},
		{Key::D8, GLFW_KEY_8},
		{Key::D9, GLFW_KEY_9},
		{Key::Semicolon, GLFW_KEY_SEMICOLON},
		{Key::Equal, GLFW_KEY_EQUAL},
		{Key::A, GLFW_KEY_A},
		{Key::B, GLFW_KEY_B},
		{Key::C, GLFW_KEY_C},
		{Key::D, GLFW_KEY_D},
		{Key::E, GLFW_KEY_E},
		{Key::F, GLFW_KEY_F},
		{Key::G, GLFW_KEY_G},
		{Key::H, GLFW_KEY_H},
		{Key::I, GLFW_KEY_I},
		{Key::J, GLFW_KEY_J},
		{Key::K, GLFW_KEY_K},
		{Key::L, GLFW_KEY_L},
		{Key::M, GLFW_KEY_M},
		{Key::N, GLFW_KEY_N},
		{Key::O, GLFW_KEY_O},
		{Key::P, GLFW_KEY_P},
		{Key::Q, GLFW_KEY_Q},
		{Key::R, GLFW_KEY_R},
		{Key::S, GLFW_KEY_S},
		{Key::T, GLFW_KEY_T},
		{Key::U, GLFW_KEY_U},
		{Key::V, GLFW_KEY_V},
		{Key::W, GLFW_KEY_W},
		{Key::X, GLFW_KEY_X},
		{Key::Y, GLFW_KEY_Y},
		{Key::Z, GLFW_KEY_Z},
		{Key::LeftBracket, GLFW_KEY_LEFT_BRACKET},
		{Key::Backslash, GLFW_KEY_BACKSLASH},
		{Key::RightBracket, GLFW_KEY_RIGHT_BRACKET},
		{Key::GraveAccent, GLFW_KEY_GRAVE_ACCENT},
		{Key::World1, GLFW_KEY_WORLD_1},
		{Key::World2, GLFW_KEY_WORLD_2},
		{Key::Escape, GLFW_KEY_ESCAPE},
		{Key::Enter, GLFW_KEY_ENTER},
		{Key::Tab, GLFW_KEY_TAB},
		{Key::Backspace, GLFW_KEY_BACKSPACE},
		{Key::Insert, GLFW_KEY_INSERT},
		{Key::Delete, GLFW_KEY_DELETE},
		{Key::Right, GLFW_KEY_RIGHT},
		{Key::Left, GLFW_KEY_LEFT},
		{Key::Down, GLFW_KEY_DOWN},
		{Key::Up, GLFW_KEY_UP},
		{Key::PageUp, GLFW_KEY_PAGE_UP},
		{Key::PageDown, GLFW_KEY_PAGE_DOWN},
		{Key::Home, GLFW_KEY_HOME},
		{Key::End, GLFW_KEY_END},
		{Key::CapsLock, GLFW_KEY_CAPS_LOCK},
		{Key::ScrollLock, GLFW_KEY_SCROLL_LOCK},
		{Key::NumLock, GLFW_KEY_NUM_LOCK},
		{Key::PrintScreen, GLFW_KEY_PRINT_SCREEN},
		{Key::Pause, GLFW_KEY_PAUSE},
		{Key::F1, GLFW_KEY_F1},
		{Key::F2, GLFW_KEY_F2},
		{Key::F3, GLFW_KEY_F3},
		{Key::F4, GLFW_KEY_F4},
		{Key::F5, GLFW_KEY_F5},
		{Key::F6, GLFW_KEY_F6},
		{Key::F7, GLFW_KEY_F7},
		{Key::F8, GLFW_KEY_F8},
		{Key::F9, GLFW_KEY_F9},
		{Key::F10, GLFW_KEY_F10},
		{Key::F11, GLFW_KEY_F11},
		{Key::F12, GLFW_KEY_F12},
		{Key::F13, GLFW_KEY_F13},
		{Key::F14, GLFW_KEY_F14},
		{Key::F15, GLFW_KEY_F15},
		{Key::F16, GLFW_KEY_F16},
		{Key::F17, GLFW_KEY_F17},
		{Key::F18, GLFW_KEY_F18},
		{Key::F19, GLFW_KEY_F19},
		{Key::F20, GLFW_KEY_F20},
		{Key::F21, GLFW_KEY_F21},
		{Key::F22, GLFW_KEY_F22},
		{Key::F23, GLFW_KEY_F23},
		{Key::F24, GLFW_KEY_F24},
		{Key::F25, GLFW_KEY_F25},
		{Key::Keypad0, GLFW_KEY_KP_0},
		{Key::Keypad1, GLFW_KEY_KP_1},
		{Key::Keypad2, GLFW_KEY_KP_2},
		{Key::Keypad3, GLFW_KEY_KP_3},
		{Key::Keypad4, GLFW_KEY_KP_4},
		{Key::Keypad5, GLFW_KEY_KP_5},
		{Key::Keypad6, GLFW_KEY_KP_6},
		{Key::Keypad7, GLFW_KEY_KP_7},
		{Key::Keypad8, GLFW_KEY_KP_8},
		{Key::Keypad9, GLFW_KEY_KP_9},
		{Key::KeypadDecimal, GLFW_KEY_KP_DECIMAL},
		{Key::KeypadDivide, GLFW_KEY_KP_DIVIDE},
		{Key::KeypadMultiply, GLFW_KEY_KP_MULTIPLY},
		{Key::KeypadSubtract, GLFW_KEY_KP_SUBTRACT},
		{Key::KeypadAdd, GLFW_KEY_KP_ADD},
		{Key::KeypadEnter, GLFW_KEY_KP_ENTER},
		{Key::KeypadEqual, GLFW_KEY_KP_EQUAL},
		{Key::LeftShift, GLFW_KEY_LEFT_SHIFT},
		{Key::LeftControl, GLFW_KEY_LEFT_CONTROL},
		{Key::LeftAlt, GLFW_KEY_LEFT_ALT},
		{Key::LeftSuper, GLFW_KEY_LEFT_SUPER},
		{Key::RightShift, GLFW_KEY_RIGHT_SHIFT},
		{Key::RightControl, GLFW_KEY_RIGHT_CONTROL},
		{Key::RightAlt, GLFW_KEY_RIGHT_ALT},
		{Key::RightSuper, GLFW_KEY_RIGHT_SUPER},
		{Key::Menu, GLFW_KEY_MENU},
	});

	// エンジンのenum順とGLFWの値を同一視しない。全ての有効Keyを1回ずつ対応付ける。
	constexpr bool HasCompleteKeyMappings() noexcept
	{
		std::array<bool, KeyCount> seen{};
		for (const auto& mapping : keyMappings)
		{
			const auto index = static_cast<std::size_t>(mapping.key);
			if (index == 0 || index >= seen.size() || seen[index])
			{
				return false;
			}
			seen[index] = true;
		}
		for (std::size_t index = 1; index < seen.size(); ++index)
		{
			if (!seen[index])
			{
				return false;
			}
		}
		return true;
	}

	static_assert(HasCompleteKeyMappings());
	static_assert(GamepadSlotCount == GLFW_JOYSTICK_LAST - GLFW_JOYSTICK_1 + 1);

	constexpr std::array<int, MouseButtonCount> mouseMappings{GLFW_MOUSE_BUTTON_LEFT, GLFW_MOUSE_BUTTON_RIGHT, GLFW_MOUSE_BUTTON_MIDDLE,
		GLFW_MOUSE_BUTTON_4, GLFW_MOUSE_BUTTON_5, GLFW_MOUSE_BUTTON_6, GLFW_MOUSE_BUTTON_7, GLFW_MOUSE_BUTTON_8};
	constexpr std::array<int, GamepadButtonCount> gamepadMappings{GLFW_GAMEPAD_BUTTON_A, GLFW_GAMEPAD_BUTTON_B, GLFW_GAMEPAD_BUTTON_X,
		GLFW_GAMEPAD_BUTTON_Y, GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER, GLFW_GAMEPAD_BUTTON_BACK,
		GLFW_GAMEPAD_BUTTON_START, GLFW_GAMEPAD_BUTTON_GUIDE, GLFW_GAMEPAD_BUTTON_LEFT_THUMB, GLFW_GAMEPAD_BUTTON_RIGHT_THUMB,
		GLFW_GAMEPAD_BUTTON_DPAD_UP, GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, GLFW_GAMEPAD_BUTTON_DPAD_DOWN, GLFW_GAMEPAD_BUTTON_DPAD_LEFT};

	void ApplyButtonAction(ButtonState& button, int action) noexcept
	{
		if (action == GLFW_PRESS && !button.down)
		{
			button.down = true;
			button.pressed = true;
		}
		else if (action == GLFW_RELEASE && button.down)
		{
			button.down = false;
			button.released = true;
		}
		// REPEAT/未知actionは新たな押下にしない。
	}

	template <std::size_t Count> void CancelButtons(std::array<ButtonState, Count>& buttons) noexcept
	{
		for (auto& button : buttons)
		{
			button.released = button.released || button.down;
			button.down = false;
			button.pressed = false;
		}
	}

	template <std::size_t Count> void ClearEdges(std::array<ButtonState, Count>& buttons) noexcept
	{
		for (auto& button : buttons)
		{
			button.pressed = false;
			button.released = false;
		}
	}

	KT::Core::Math::Vector2 ApplyStickDeadZone(float x, float y, float deadZone) noexcept
	{
		// GLFW gamepadの+Y downをエンジンの+Y upへ変換する。
		const double length = std::hypot(static_cast<double>(x), static_cast<double>(y));
		if (length <= deadZone)
		{
			return {};
		}
		const double magnitude = ((std::min)(length, 1.0) - deadZone) / (1.0 - deadZone);
		const double scale = magnitude / length;
		return {static_cast<float>(x * scale), static_cast<float>(-y * scale)};
	}

	bool IsUsableGamepadSample(const GLFWgamepadstate& state) noexcept
	{
		for (const float axis : state.axes)
		{
			if (!std::isfinite(axis) || axis < -1.0f || axis > 1.0f)
			{
				return false;
			}
		}
		for (const unsigned char button : state.buttons)
		{
			if (button != GLFW_PRESS && button != GLFW_RELEASE)
			{
				return false;
			}
		}
		return true;
	}
}

namespace KT::Platform
{
	Input::Input(Window& window)
		: window_(window)
	{
		// 二重接続はWindow/入力状態を変更する前に拒否する。
		window_.AttachInput(*this);
		focused_ = glfwGetWindowAttrib(window_.window_, GLFW_FOCUSED) == GLFW_TRUE;
		gamepadNeedsBaseline_.fill(true);
		SampleKeyboardAndMouseBaseline();
		ResetCursorBaseline();
		snapshot_.focused = focused_;
		snapshot_.keyboard = pendingKeyboard_;
		snapshot_.mouse = pendingMouse_;
	}

	Input::~Input()
	{
		window_.DetachInput(*this);
		if (cursorMode_ == CursorMode::Captured)
		{
			glfwSetInputMode(window_.window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}

	void Input::Update()
	{
		InputSnapshot next{};
		next.focused = focused_;
		next.cursorMode = cursorMode_;
		next.keyboard = pendingKeyboard_;
		next.mouse = pendingMouse_;
		SampleGamepads(next);
		snapshot_ = next;

		// 公開後に未公開edge/区間量だけ消す。押下状態と位置は次区間へ引き継ぐ。
		ClearEdges(pendingKeyboard_.keys);
		ClearEdges(pendingMouse_.buttons);
		pendingMouse_.deltaX = 0;
		pendingMouse_.deltaY = 0;
		pendingMouse_.scrollX = 0;
		pendingMouse_.scrollY = 0;
		for (auto& releases : gamepadReleasePending_)
		{
			releases.fill(false);
		}
	}

	const InputSnapshot& Input::GetSnapshot() const noexcept
	{
		return snapshot_;
	}

	void Input::SetCursorMode(CursorMode mode)
	{
		int nativeMode = GLFW_CURSOR_NORMAL;
		switch (mode)
		{
		case CursorMode::Normal:
			break;
		case CursorMode::Captured:
			nativeMode = GLFW_CURSOR_DISABLED;
			break;
		default:
			throw std::invalid_argument("CursorModeが不正です。");
		}
		if (mode == cursorMode_)
		{
			return;
		}

		glfwSetInputMode(window_.window_, GLFW_CURSOR, nativeMode);
		if (glfwGetInputMode(window_.window_, GLFW_CURSOR) != nativeMode)
		{
			throw std::runtime_error("カーソルモードの変更に失敗しました。");
		}
		cursorMode_ = mode;
		ResetCursorBaseline();
	}

	void Input::SetStickDeadZone(float deadZone)
	{
		if (!std::isfinite(deadZone) || deadZone < 0 || deadZone >= 1)
		{
			throw std::invalid_argument("スティックのデッドゾーンはfiniteかつ[0,1)が必要です。");
		}
		stickDeadZone_ = deadZone;
	}

	void Input::OnKey(int key, int action) noexcept
	{
		if (!focused_)
		{
			return;
		}
		for (const auto& mapping : keyMappings)
		{
			if (mapping.nativeKey == key)
			{
				ApplyButtonAction(pendingKeyboard_.keys[static_cast<std::size_t>(mapping.key)], action);
				return;
			}
		}
		// GLFW_KEY_UNKNOWN等、初版で対応しないキーは無視する。
	}

	void Input::OnMouseButton(int button, int action) noexcept
	{
		if (!focused_)
		{
			return;
		}
		for (std::size_t index = 0; index < mouseMappings.size(); ++index)
		{
			if (mouseMappings[index] == button)
			{
				ApplyButtonAction(pendingMouse_.buttons[index], action);
				return;
			}
		}
	}

	void Input::OnCursorPosition(double x, double y) noexcept
	{
		if (!std::isfinite(x) || !std::isfinite(y))
		{
			return;
		}
		if (focused_ && cursorBaselineValid_)
		{
			const double nextX = pendingMouse_.deltaX + (x - pendingMouse_.x);
			const double nextY = pendingMouse_.deltaY + (y - pendingMouse_.y);
			// 非finiteな区間量を公開しない。異常差分は捨て、位置の基準だけ取り直す。
			if (std::isfinite(nextX) && std::isfinite(nextY))
			{
				pendingMouse_.deltaX = nextX;
				pendingMouse_.deltaY = nextY;
			}
		}
		pendingMouse_.x = x;
		pendingMouse_.y = y;
		cursorBaselineValid_ = focused_;
	}

	void Input::OnScroll(double x, double y) noexcept
	{
		if (!focused_ || !std::isfinite(x) || !std::isfinite(y))
		{
			return;
		}
		const double nextX = pendingMouse_.scrollX + x;
		const double nextY = pendingMouse_.scrollY + y;
		if (std::isfinite(nextX) && std::isfinite(nextY))
		{
			pendingMouse_.scrollX = nextX;
			pendingMouse_.scrollY = nextY;
		}
	}

	void Input::OnFocus(bool focused) noexcept
	{
		if (focused_ == focused)
		{
			return;
		}
		focused_ = focused;
		gamepadNeedsBaseline_.fill(true);
		if (!focused_)
		{
			CancelButtons(pendingKeyboard_.keys);
			CancelButtons(pendingMouse_.buttons);
			for (std::size_t slot = 0; slot < GamepadSlotCount; ++slot)
			{
				for (std::size_t button = 0; button < GamepadButtonCount; ++button)
				{
					gamepadReleasePending_[slot][button] =
						gamepadReleasePending_[slot][button] || snapshot_.gamepads[slot].buttons[button].down;
				}
			}
			pendingMouse_.deltaX = 0;
			pendingMouse_.deltaY = 0;
			pendingMouse_.scrollX = 0;
			pendingMouse_.scrollY = 0;
			cursorBaselineValid_ = false;
		}
		else
		{
			// 復帰時の状態を基準にする。保持中の操作からPressedを捏造しない。
			SampleKeyboardAndMouseBaseline();
			ResetCursorBaseline();
		}
	}

	void Input::ResetCursorBaseline() noexcept
	{
		double x = 0;
		double y = 0;
		glfwGetCursorPos(window_.window_, &x, &y);
		cursorBaselineValid_ = focused_ && std::isfinite(x) && std::isfinite(y);
		if (std::isfinite(x) && std::isfinite(y))
		{
			pendingMouse_.x = x;
			pendingMouse_.y = y;
		}
		pendingMouse_.deltaX = 0;
		pendingMouse_.deltaY = 0;
	}

	void Input::SampleKeyboardAndMouseBaseline() noexcept
	{
		for (const auto& mapping : keyMappings)
		{
			auto& button = pendingKeyboard_.keys[static_cast<std::size_t>(mapping.key)];
			button.down = focused_ && glfwGetKey(window_.window_, mapping.nativeKey) == GLFW_PRESS;
			button.pressed = false;
		}
		for (std::size_t index = 0; index < mouseMappings.size(); ++index)
		{
			auto& button = pendingMouse_.buttons[index];
			button.down = focused_ && glfwGetMouseButton(window_.window_, mouseMappings[index]) == GLFW_PRESS;
			button.pressed = false;
		}
	}

	void Input::SampleGamepads(InputSnapshot& next) noexcept
	{
		for (std::size_t slot = 0; slot < GamepadSlotCount; ++slot)
		{
			const int joystick = GLFW_JOYSTICK_1 + static_cast<int>(slot);
			auto& output = next.gamepads[slot];
			const auto& previous = snapshot_.gamepads[slot];
			GLFWgamepadstate native{};
			if (glfwJoystickPresent(joystick) != GLFW_TRUE)
			{
				output.status = GamepadStatus::Disconnected;
			}
			else if (glfwJoystickIsGamepad(joystick) != GLFW_TRUE)
			{
				output.status = GamepadStatus::Unmapped;
			}
			else if (glfwGetGamepadState(joystick, &native) == GLFW_TRUE && IsUsableGamepadSample(native))
			{
				output.status = GamepadStatus::Ready;
			}
			else
			{
				// 問合せ途中の切断も再確認する。失敗時に以前の操作値を残さない。
				if (glfwJoystickPresent(joystick) != GLFW_TRUE)
				{
					output.status = GamepadStatus::Disconnected;
				}
				else if (glfwJoystickIsGamepad(joystick) != GLFW_TRUE)
				{
					output.status = GamepadStatus::Unmapped;
				}
				else
				{
					output.status = GamepadStatus::ReadFailed;
				}
			}

			if (output.status != GamepadStatus::Ready || !focused_)
			{
				for (std::size_t button = 0; button < GamepadButtonCount; ++button)
				{
					output.buttons[button].released = previous.buttons[button].down || gamepadReleasePending_[slot][button];
				}
				gamepadNeedsBaseline_[slot] = true;
				continue;
			}

			const bool baseline = gamepadNeedsBaseline_[slot] || previous.status != GamepadStatus::Ready;
			for (std::size_t button = 0; button < GamepadButtonCount; ++button)
			{
				auto& state = output.buttons[button];
				state.down = native.buttons[gamepadMappings[button]] == GLFW_PRESS;
				state.pressed = !baseline && state.down && !previous.buttons[button].down;
				state.released = gamepadReleasePending_[slot][button] || (!baseline && !state.down && previous.buttons[button].down);
			}
			output.leftStick =
				ApplyStickDeadZone(native.axes[GLFW_GAMEPAD_AXIS_LEFT_X], native.axes[GLFW_GAMEPAD_AXIS_LEFT_Y], stickDeadZone_);
			output.rightStick =
				ApplyStickDeadZone(native.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], native.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y], stickDeadZone_);
			output.leftTrigger = (native.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] + 1.0f) * 0.5f;
			output.rightTrigger = (native.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] + 1.0f) * 0.5f;
			gamepadNeedsBaseline_[slot] = false;
		}
	}
}
