#include "InputManager.h"

namespace Horizon {
	void InputManager::Init(Window* window, Camera* camera)
	{
		mWindow = window;
		mCamera = camera;
		lastX = window->getWidth() / 2.0f;
		lastY = window->getHeight() / 2.0f;
		firstMouse = true;
		yaw = -90.0f;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
		pitch = 0.0f;
	}
	void InputManager::processInput()
	{
		//processMouseInput();
		processKeyboardInput();
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

		float xpos = static_cast<float>(xposIn);
		float ypos = static_cast<float>(yposIn);

		if (firstMouse)
		{
			lastX = xpos;
			lastY = ypos;
			firstMouse = false;
		}

		float xoffset = xpos - lastX;
		float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
		lastX = xpos;
		lastY = ypos;
		//spdlog::info("{} {}", xoffset, yoffset);
		float sensitivity = 0.1f; // change this value to your liking
		xoffset *= sensitivity;
		yoffset *= sensitivity;

		yaw += xoffset;
		pitch += yoffset;

		// make sure that when pitch is out of bounds, screen doesn't get flipped
		if (pitch > 89.0f)
			pitch = 89.0f;
		if (pitch < -89.0f)
			pitch = -89.0f;

		glm::vec3 front;
		front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		front.y = sin(glm::radians(pitch));
		front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		mCamera->setLookAt(mCamera->getPosition(), mCamera->getPosition() + normalize(front));

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

}
