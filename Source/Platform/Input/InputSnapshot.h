#pragma once
#include "InputTypes.h"
#include <Core/Math/Vector2.h>
#include <array>
#include <stdexcept>

namespace KT::Platform
{
	struct KeyboardState
	{
		std::array<ButtonState, KeyCount> keys{};

		const ButtonState& GetKey(Key key) const
		{
			const auto index = static_cast<std::size_t>(key);
			if (index == 0 || index >= keys.size())
			{
				throw std::invalid_argument("Keyが不正です。");
			}
			return keys[index];
		}
	};

	struct MouseState
	{
		std::array<ButtonState, MouseButtonCount> buttons{};
		// 内容領域の座標。左上原点、右が+X、下が+Y。Framebuffer pixelではない。
		// Captured中は仮想座標。位置/移動/scrollのdouble精度を保つ。
		double x = 0;
		double y = 0;
		double deltaX = 0;
		double deltaY = 0;
		double scrollX = 0;
		double scrollY = 0;

		const ButtonState& GetButton(MouseButton button) const
		{
			const auto index = static_cast<std::size_t>(button);
			if (index >= buttons.size())
			{
				throw std::invalid_argument("MouseButtonが不正です。");
			}
			return buttons[index];
		}
	};

	struct GamepadState
	{
		GamepadStatus status = GamepadStatus::Disconnected;
		std::array<ButtonState, GamepadButtonCount> buttons{};
		// デッドゾーン適用後。右が+X、上が+Y、長さは最大1。
		KT::Core::Math::Vector2 leftStick{};
		KT::Core::Math::Vector2 rightStick{};
		// GLFWの対応軸を[-1,1]から[0,1]へ変換する。
		// GLFWは未対応軸を0で返すため、その場合と半押し(0.5)の区別はできない。
		float leftTrigger = 0;
		float rightTrigger = 0;

		const ButtonState& GetButton(GamepadButton button) const
		{
			const auto index = static_cast<std::size_t>(button);
			if (index >= buttons.size())
			{
				throw std::invalid_argument("GamepadButtonが不正です。");
			}
			return buttons[index];
		}
	};

	// 値としてコピー可能。GetSnapshotのconst参照は次Updateで内容が置き換わる。
	struct InputSnapshot
	{
		bool focused = false;
		CursorMode cursorMode = CursorMode::Normal;
		KeyboardState keyboard{};
		MouseState mouse{};
		std::array<GamepadState, GamepadSlotCount> gamepads{};

		const GamepadState& GetGamepad(std::size_t slot) const
		{
			if (slot >= gamepads.size())
			{
				throw std::out_of_range("Gamepadスロットが範囲外です。");
			}
			return gamepads[slot];
		}
	};
}
