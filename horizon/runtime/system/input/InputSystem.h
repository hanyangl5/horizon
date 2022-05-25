#pragma once

#include <runtime/core/math/Math.h>
#include <runtime/core/window/Window.h>
#include <runtime/function/scene/camera/Camera.h>

namespace Horizon {

	class InputSystem {
	public:
		enum class Key {
			ESCAPE,
			SPACE,
			KEY_W,
			KEY_S,
			KEY_A,
			KEY_D,
			KEY_LCTRL,
			KEY_LSHIFT
		};

		enum class MouseButton {
			LEFT_BUTTON,
			RIGHT_BUTTON
		};

		enum class INPUT_STATE {
			PRESS,
			RELEASE
		};
	public:
		InputSystem(std::shared_ptr<Window> window, std::shared_ptr<Camera> camera) noexcept;

		~InputSystem() noexcept;

		void Tick() noexcept;

	private:

		void ProcessKeyboardInput() noexcept;

		void ProcessMouseInput() noexcept;

		bool GetKeyPress(Key key) const noexcept;

		int GetMouseButtonPress(MouseButton button) const noexcept;

		int GetMouseButtonRelease(MouseButton button) const noexcept;

	private:

		std::shared_ptr<Window> m_window = nullptr;
		std::shared_ptr<Camera> m_camera = nullptr;

		f32 m_last_x;
		f32 m_last_y;
		f32 m_mouse_sensitivity_x = 1.0f;
		f32 m_mouse_sensitivity_y = 1.0f;
		bool m_first_mouse;

	};
}