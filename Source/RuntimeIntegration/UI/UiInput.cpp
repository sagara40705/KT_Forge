#include <RuntimeIntegration/UI/UiInput.h>
#include <RmlUi/Core.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace KT::RuntimeIntegration
{
	namespace P = KT::Platform;

	namespace
	{
		// エンジンのenumと上流のenumは、対応が確認できる連続範囲だけ変換する。
		Rml::Input::KeyIdentifier UiKey(P::Key key)
		{
			using namespace Rml::Input;
			const auto value = static_cast<unsigned>(key);
			if (key >= P::Key::A && key <= P::Key::Z)
			{
				return static_cast<KeyIdentifier>(KI_A + value - static_cast<unsigned>(P::Key::A));
			}
			if (key >= P::Key::D0 && key <= P::Key::D9)
			{
				return static_cast<KeyIdentifier>(KI_0 + value - static_cast<unsigned>(P::Key::D0));
			}
			switch (key)
			{
			case P::Key::Escape:
				return KI_ESCAPE;
			case P::Key::Enter:
				return KI_RETURN;
			case P::Key::Space:
				return KI_SPACE;
			case P::Key::Tab:
				return KI_TAB;
			case P::Key::Backspace:
				return KI_BACK;
			case P::Key::Delete:
				return KI_DELETE;
			case P::Key::Left:
				return KI_LEFT;
			case P::Key::Right:
				return KI_RIGHT;
			case P::Key::Up:
				return KI_UP;
			case P::Key::Down:
				return KI_DOWN;
			case P::Key::Home:
				return KI_HOME;
			case P::Key::End:
				return KI_END;
			case P::Key::LeftShift:
				return KI_LSHIFT;
			case P::Key::RightShift:
				return KI_RSHIFT;
			case P::Key::LeftControl:
				return KI_LCONTROL;
			case P::Key::RightControl:
				return KI_RCONTROL;
			case P::Key::LeftAlt:
				return KI_LMENU;
			case P::Key::RightAlt:
				return KI_RMENU;
			default:
				return KI_UNKNOWN;
			}
		}

		void Merge(P::ButtonState& pending, const P::ButtonState& current)
		{
			pending.down = current.down;
			pending.pressed = pending.pressed || current.pressed;
			pending.released = pending.released || current.released;
		}
	}

	void UiInput::SetFrameKeys(std::span<const P::Key> keys)
	{
		std::array<bool, P::KeyCount> candidate{};
		for (const auto key : keys)
		{
			const auto index = static_cast<std::size_t>(key);
			if (index == 0 || index >= candidate.size())
			{
				throw std::invalid_argument("外側フレームへ予約するKeyが不正です。");
			}
			candidate[index] = true;
		}
		frameKeys_ = candidate;
	}

	const P::InputSnapshot& UiInput::FrameInput() const noexcept
	{
		return frame_;
	}

	void UiInput::Route(Rml::Context* context, const P::InputSnapshot& raw, int windowWidth, int windowHeight, bool modal)
	{
		const auto dimensions = context ? context->GetDimensions() : Rml::Vector2i{1, 1};
		frame_ = {};
		frame_.focused = raw.focused;
		frame_.cursorMode = raw.cursorMode;
		int modifiers = 0;
		if (raw.keyboard.GetKey(P::Key::LeftShift).down || raw.keyboard.GetKey(P::Key::RightShift).down)
		{
			modifiers |= Rml::Input::KM_SHIFT;
		}
		if (raw.keyboard.GetKey(P::Key::LeftControl).down || raw.keyboard.GetKey(P::Key::RightControl).down)
		{
			modifiers |= Rml::Input::KM_CTRL;
		}
		if (raw.keyboard.GetKey(P::Key::LeftAlt).down || raw.keyboard.GetKey(P::Key::RightAlt).down)
		{
			modifiers |= Rml::Input::KM_ALT;
		}

		// Content座標をFramebufferへ変換する。focusを失った回はUI hoverも解除する。
		bool mouseCaptured = modal;
		if (context && raw.focused && raw.cursorMode == P::CursorMode::Normal && windowWidth > 0 && windowHeight > 0)
		{
			const auto x = (std::clamp)(raw.mouse.x * dimensions.x / windowWidth, -32768.0, 32767.0);
			const auto y = (std::clamp)(raw.mouse.y * dimensions.y / windowHeight, -32768.0, 32767.0);
			mouseCaptured = !context->ProcessMouseMove(static_cast<int>(x), static_cast<int>(y), modifiers) || modal;
		}
		else if (context)
		{
			context->ProcessMouseLeave();
		}
		for (std::size_t index = 0; index < P::MouseButtonCount; ++index)
		{
			const auto& state = raw.mouse.buttons[index];
			bool consumed = mouseCaptured;
			if (context && state.pressed)
			{
				consumed = !context->ProcessMouseButtonDown(static_cast<int>(index), modifiers) || consumed;
			}
			if (context && state.released)
			{
				consumed = !context->ProcessMouseButtonUp(static_cast<int>(index), modifiers) || consumed;
			}
			if (consumed || blockedMouse_[index] || !raw.focused)
			{
				pending_.mouse.buttons[index] = {};
				blockedMouse_[index] = state.down;
			}
			else
			{
				Merge(pending_.mouse.buttons[index], state);
			}
		}
		// 同じ入力回のmouse押下で変わったfocusを確定してから、文字キーを配送する。
		auto* focus = context ? context->GetFocusElement() : nullptr;
		const bool keyboardCaptured = modal || (focus && focus != context->GetRootElement() && focus->GetTagName() != "body");
		// input/textarea内の編集キーを、アプリの予約キーとして横取りしない。
		bool textEditing = false;
		for (auto* element = focus; element && element != context->GetRootElement(); element = element->GetParentNode())
		{
			textEditing = textEditing || element->GetTagName() == "input" || element->GetTagName() == "textarea";
		}
		pending_.focused = raw.focused;
		pending_.cursorMode = raw.cursorMode;

		for (std::size_t index = 1; index < P::KeyCount; ++index)
		{
			const auto key = static_cast<P::Key>(index);
			const auto& state = raw.keyboard.keys[index];
			if (frameKeys_[index] && !textEditing)
			{
				if (raw.focused)
				{
					frame_.keyboard.keys[index] = state;
				}
				pending_.keyboard.keys[index] = {};
				blockedKeys_[index] = state.down;
				continue;
			}
			bool consumed = keyboardCaptured;
			const auto mapped = UiKey(key);
			if (context && mapped != Rml::Input::KI_UNKNOWN)
			{
				if (state.pressed)
				{
					consumed = !context->ProcessKeyDown(mapped, modifiers) || consumed;
				}
				if (state.released)
				{
					consumed = !context->ProcessKeyUp(mapped, modifiers) || consumed;
				}
			}
			if (consumed || blockedKeys_[index] || !raw.focused)
			{
				pending_.keyboard.keys[index] = {};
				blockedKeys_[index] = state.down;
			}
			else
			{
				Merge(pending_.keyboard.keys[index], state);
			}
		}
		if (context && (raw.mouse.scrollX != 0 || raw.mouse.scrollY != 0))
		{
			mouseCaptured =
				!context->ProcessMouseWheel({static_cast<float>(raw.mouse.scrollX), static_cast<float>(-raw.mouse.scrollY)}, modifiers) ||
				mouseCaptured;
		}
		pending_.mouse.x = raw.mouse.x;
		pending_.mouse.y = raw.mouse.y;
		if (mouseCaptured || !raw.focused)
		{
			pending_.mouse.deltaX = pending_.mouse.deltaY = pending_.mouse.scrollX = pending_.mouse.scrollY = 0;
		}
		else
		{
			pending_.mouse.deltaX += raw.mouse.deltaX;
			pending_.mouse.deltaY += raw.mouse.deltaY;
			pending_.mouse.scrollX += raw.mouse.scrollX;
			pending_.mouse.scrollY += raw.mouse.scrollY;
		}
		for (std::size_t slot = 0; slot < P::GamepadSlotCount; ++slot)
		{
			const auto& pad = raw.gamepads[slot];
			auto& captured = blockedPads_[slot];
			auto& current = pending_.gamepads[slot];
			current.status = pad.status;

			// ボタンと各軸を独立して解除し、閉じた後の新しい操作だけを通す。
			for (std::size_t index = 0; index < P::GamepadButtonCount; ++index)
			{
				if (modal || captured.buttons[index] || !raw.focused)
				{
					current.buttons[index] = {};
					captured.buttons[index] = pad.buttons[index].down;
				}
				else
				{
					Merge(current.buttons[index], pad.buttons[index]);
				}
			}
			if (modal || captured.leftStick || !raw.focused)
			{
				current.leftStick = {};
				captured.leftStick = pad.leftStick.x != 0 || pad.leftStick.y != 0;
			}
			else
			{
				current.leftStick = pad.leftStick;
			}
			if (modal || captured.rightStick || !raw.focused)
			{
				current.rightStick = {};
				captured.rightStick = pad.rightStick.x != 0 || pad.rightStick.y != 0;
			}
			else
			{
				current.rightStick = pad.rightStick;
			}
			if (modal || captured.leftTrigger || !raw.focused)
			{
				current.leftTrigger = 0;
				captured.leftTrigger = pad.leftTrigger > 0.05f;
			}
			else
			{
				current.leftTrigger = pad.leftTrigger;
			}
			if (modal || captured.rightTrigger || !raw.focused)
			{
				current.rightTrigger = 0;
				captured.rightTrigger = pad.rightTrigger > 0.05f;
			}
			else
			{
				current.rightTrigger = pad.rightTrigger;
			}
		}
	}

	void UiInput::Consume() noexcept
	{
		for (auto& button : pending_.keyboard.keys)
		{
			button.pressed = button.released = false;
		}
		for (auto& button : pending_.mouse.buttons)
		{
			button.pressed = button.released = false;
		}
		for (auto& pad : pending_.gamepads)
		{
			for (auto& button : pad.buttons)
			{
				button.pressed = button.released = false;
			}
		}
		pending_.mouse.deltaX = pending_.mouse.deltaY = pending_.mouse.scrollX = pending_.mouse.scrollY = 0;
	}

	const P::InputSnapshot& UiInput::GameInput() const noexcept
	{
		return pending_;
	}
}
