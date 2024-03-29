#include "InputManager.h"

namespace Horizon {

InputManager::InputManager(std::shared_ptr<Window> window, std::shared_ptr<Camera> camera) noexcept
    : m_window(window), m_camera(camera) {
    m_window = window;
    m_camera = camera;
    m_last_x = window->getWidth() / 2.0f;
    m_last_y = window->getHeight() / 2.0f;
    m_first_mouse = true;
}
InputManager::~InputManager() noexcept {}
void InputManager::ProcessInput() noexcept {
    glfwPollEvents();
    ProcessMouseInput();
    ProcessKeyboardInput();
    m_camera->UpdateViewMatrix();
}

void InputManager::ProcessKeyboardInput() noexcept {

    if (GetKeyPress(Key::ESCAPE)) {
        m_window->close();
    }
    if (GetKeyPress(Key::KEY_W)) {
        m_camera->Move(Direction::FORWARD);
    }
    if (GetKeyPress(Key::KEY_S)) {
        m_camera->Move(Direction::BACKWARD);
    }
    if (GetKeyPress(Key::KEY_A)) {
        m_camera->Move(Direction::LEFT);
    }
    if (GetKeyPress(Key::KEY_D)) {
        m_camera->Move(Direction::RIGHT);
    }
    if (GetKeyPress(Key::SPACE)) {
        m_camera->Move(Direction::UP);
    }
    if (GetKeyPress(Key::KEY_LCTRL)) {
        m_camera->Move(Direction::DOWN);
    }
}

void InputManager::ProcessMouseInput() noexcept {
    f64 xposIn, yposIn;
    glfwGetCursorPos(m_window->getWindow(), &xposIn, &yposIn);

    f32 xpos = static_cast<f32>(xposIn);
    f32 ypos = static_cast<f32>(yposIn);

    if (m_first_mouse) {
        m_last_x = xpos;
        m_last_y = ypos;
        m_first_mouse = false;
    }

    f32 xoffset = xpos - m_last_x;
    f32 yoffset = ypos - m_last_y; // reversed since y-coordinates go from bottom to top

    m_last_x = xpos;
    m_last_y = ypos;

    if (GetMouseButtonPress(MouseButton::RIGHT_BUTTON)) {

        xoffset *= m_mouse_sensitivity_x;
        yoffset *= m_mouse_sensitivity_y;

        m_camera->Rotate(xoffset, yoffset);

    } else if (GetMouseButtonRelease(MouseButton::RIGHT_BUTTON)) {
        m_first_mouse = true;
    }
}

bool InputManager::GetKeyPress(Key inputKey) const noexcept {
    switch (inputKey) {
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

int InputManager::GetMouseButtonPress(MouseButton button) const noexcept {
    switch (button) {
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

int InputManager::GetMouseButtonRelease(MouseButton button) const noexcept {
    switch (button) {
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

} // namespace Horizon
