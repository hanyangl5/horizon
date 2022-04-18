#include "InputManager.h"

namespace Horizon {

	InputManager::InputManager(std::shared_ptr<Window> window, std::shared_ptr<Camera> camera) :m_window(window), m_camera(camera)
	{
		m_window = window;
		m_camera = camera;
		m_last_x = window->getWidth() / 2.0f;
		m_last_y = window->getHeight() / 2.0f;
		m_first_mouse = true;
	}
	InputManager::~InputManager()
	{
	}
	void InputManager::processInput()
	{

		processMouseInput();
		processKeyboardInput();
		m_camera->updateViewMatrix();
	}

	void InputManager::processKeyboardInput()
	{

		if (getKeyPress(Key::ESCAPE)) {
			m_window->close();
		}
		if (getKeyPress(Key::KEY_W)) {
			m_camera->move(Direction::FORWARD);
		}
		if (getKeyPress(Key::KEY_S)) {
			m_camera->move(Direction::BACKWARD);
		}
		if (getKeyPress(Key::KEY_A)) {
			m_camera->move(Direction::LEFT);
		}
		if (getKeyPress(Key::KEY_D)) {
			m_camera->move(Direction::RIGHT);
		}
		if (getKeyPress(Key::SPACE)) {
			m_camera->move(Direction::UP);
		}
		if (getKeyPress(Key::KEY_LCTRL)) {
			m_camera->move(Direction::DOWN);
		}

	}


	void InputManager::processMouseInput()
	{
		f64 xposIn, yposIn;
		glfwGetCursorPos(m_window->getWindow(), &xposIn, &yposIn);

		f32 xpos = static_cast<f32>(xposIn);
		f32 ypos = static_cast<f32>(yposIn);

		if (m_first_mouse)
		{
			m_last_x = xpos;
			m_last_y = ypos;
			m_first_mouse = false;
		}

		f32 xoffset = xpos - m_last_x;
		f32 yoffset = ypos - m_last_y; // reversed since y-coordinates go from bottom to top

		m_last_x = xpos;
		m_last_y = ypos;

		if (getMouseButtonPress(MouseButton::RIGHT_BUTTON)) {

			xoffset *= m_mouse_sensitivity_x;
			yoffset *= m_mouse_sensitivity_y;

			m_camera->rotate(xoffset, yoffset);

		}
		else if (getMouseButtonRelease(MouseButton::RIGHT_BUTTON)) {
			m_first_mouse = true;
		}
	}

	bool InputManager::getKeyPress(Key inputKey)
	{
		switch (inputKey)
		{
		case Key::ESCAPE:
			return glfwGetKey(m_window->getWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS;
			break;
		case Key::SPACE:
			return glfwGetKey(m_window->getWindow(), GLFW_KEY_SPACE) == GLFW_PRESS;
			break;
		case Key::KEY_W:
			return glfwGetKey(m_window->getWindow(), GLFW_KEY_W) == GLFW_PRESS;
			break;
		case Key::KEY_S:
			return glfwGetKey(m_window->getWindow(), GLFW_KEY_S) == GLFW_PRESS;
			break;
		case Key::KEY_A:
			return glfwGetKey(m_window->getWindow(), GLFW_KEY_A) == GLFW_PRESS;
			break;
		case Key::KEY_D:
			return glfwGetKey(m_window->getWindow(), GLFW_KEY_D) == GLFW_PRESS;
			break;
		case Key::KEY_LCTRL:
			return glfwGetKey(m_window->getWindow(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
			break;
		case Key::KEY_LSHIFT:
			return glfwGetKey(m_window->getWindow(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
			break;
		default:
			return false;
			break;
		}
	}

	int InputManager::getMouseButtonPress(MouseButton button)
	{
		switch (button)
		{
		case Horizon::InputManager::MouseButton::LEFT_BUTTON:
			return glfwGetMouseButton(m_window->getWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
			break;
		case Horizon::InputManager::MouseButton::RIGHT_BUTTON:
			return glfwGetMouseButton(m_window->getWindow(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
			break;
		default:
			return 0;
			break;
		}

	}

	int InputManager::getMouseButtonRelease(MouseButton button)
	{
		switch (button)
		{
		case Horizon::InputManager::MouseButton::LEFT_BUTTON:
			return glfwGetMouseButton(m_window->getWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE;
			break;
		case Horizon::InputManager::MouseButton::RIGHT_BUTTON:
			return glfwGetMouseButton(m_window->getWindow(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE;
			break;
		default:
			return 0;
			break;
		}
	}

}
