#include "InputManager.h"

namespace Horizon {
	void InputManager::Init(Window* window, Camera* camera)
	{
		mWindow = window;
		mCamera = camera;
		lastX = window->getWidth() / 2.0f;
		lastY = window->getHeight() / 2.0f;
		firstMouse = true;
	}
	void InputManager::processInput()
	{

		processMouseInput();
		processKeyboardInput();
		mCamera->updateViewMatrix();
	}

	void InputManager::processKeyboardInput()
	{

		if (getKeyPress(Key::ESCAPE)) {
			mWindow->close();
		}
		if (getKeyPress(Key::KEY_W)) {
			mCamera->move(Direction::FORWARD);
		}
		if (getKeyPress(Key::KEY_S)) {
			mCamera->move(Direction::BACKWARD);
		}
		if (getKeyPress(Key::KEY_A)) {
			mCamera->move(Direction::LEFT);
		}
		if (getKeyPress(Key::KEY_D)) {
			mCamera->move(Direction::RIGHT);
		}
		if (getKeyPress(Key::SPACE)) {
			mCamera->move(Direction::UP);
		}
		if (getKeyPress(Key::KEY_LCTRL)) {
			mCamera->move(Direction::DOWN);
		}

	}


	void InputManager::processMouseInput()
	{
		f64 xposIn, yposIn;
		glfwGetCursorPos(mWindow->getWindow(), &xposIn, &yposIn);

		f32 xpos = static_cast<f32>(xposIn);
		f32 ypos = static_cast<f32>(yposIn);

		if (firstMouse)
		{
			lastX = xpos;
			lastY = ypos;
			firstMouse = false;
		}

		f32 xoffset = xpos - lastX;
		f32 yoffset = ypos - lastY; // reversed since y-coordinates go from bottom to top
		
		lastX = xpos;
		lastY = ypos;

		if (getMouseButtonPress(MouseButton::RIGHT_BUTTON)) {

			xoffset *= mMouseSensitivityX;
			yoffset *= mMouseSensitivityY;

			mCamera->rotate(xoffset, yoffset);

		}
		else if (getMouseButtonRelease(MouseButton::RIGHT_BUTTON)) {
			firstMouse = true;
		}
	}

	bool InputManager::getKeyPress(Key inputKey)
	{
		switch (inputKey)
		{
		case Key::ESCAPE:
			return glfwGetKey(mWindow->getWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS;
			break;
		case Key::SPACE:
			return glfwGetKey(mWindow->getWindow(), GLFW_KEY_SPACE) == GLFW_PRESS;
			break;
		case Key::KEY_W:
			return glfwGetKey(mWindow->getWindow(), GLFW_KEY_W) == GLFW_PRESS;
			break;
		case Key::KEY_S:
			return glfwGetKey(mWindow->getWindow(), GLFW_KEY_S) == GLFW_PRESS;
			break;
		case Key::KEY_A:
			return glfwGetKey(mWindow->getWindow(), GLFW_KEY_A) == GLFW_PRESS;
			break;
		case Key::KEY_D:
			return glfwGetKey(mWindow->getWindow(), GLFW_KEY_D) == GLFW_PRESS;
			break;
		case Key::KEY_LCTRL:
			return glfwGetKey(mWindow->getWindow(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
			break;
		case Key::KEY_LSHIFT:
			return glfwGetKey(mWindow->getWindow(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
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
			return glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
			break;
		case Horizon::InputManager::MouseButton::RIGHT_BUTTON:
			return glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
			break;
		default:
			break;
		}

	}

	int InputManager::getMouseButtonRelease(MouseButton button)
	{
		switch (button)
		{
		case Horizon::InputManager::MouseButton::LEFT_BUTTON:
			return glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE;
			break;
		case Horizon::InputManager::MouseButton::RIGHT_BUTTON:
			return glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE;
			break;
		default:
			break;
		}
	}

}
