#include "InputSystem.h"

namespace Horizon {

InputSystem::InputSystem(Window *window) noexcept : m_window(window) {
    m_window = window;
    m_last_x = window->GetWidth() / 2.0f;
    m_last_y = window->GetHeight() / 2.0f;
    m_first_mouse = true;
}
InputSystem::~InputSystem() noexcept {}

void InputSystem::SetCamera(Camera *camera) noexcept { m_camera = camera; }

void InputSystem::Tick() {

    assert(m_camera != nullptr);
    assert(m_camera->GetCameraSpeed() != 0);

    glfwPollEvents();
    ProcessMouseInput();
    ProcessKeyboardInput();
    m_camera->UpdateViewMatrix();
}

void InputSystem::ProcessKeyboardInput() {


    if (GetKeyPress(Key::ESCAPE)) {
        m_window->Close();
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

void InputSystem::ProcessMouseInput() {
    f64 xposIn, yposIn;
    glfwGetCursorPos(m_window->GetWindow(), &xposIn, &yposIn);

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

bool InputSystem::GetKeyPress(Key inputKey) const {
    switch (inputKey) {
    case Key::ESCAPE:
        return glfwGetKey(m_window->GetWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS;
        break;
    case Key::SPACE:
        return glfwGetKey(m_window->GetWindow(), GLFW_KEY_SPACE) == GLFW_PRESS;
        break;
    case Key::KEY_W:
        return glfwGetKey(m_window->GetWindow(), GLFW_KEY_W) == GLFW_PRESS;
        break;
    case Key::KEY_S:
        return glfwGetKey(m_window->GetWindow(), GLFW_KEY_S) == GLFW_PRESS;
        break;
    case Key::KEY_A:
        return glfwGetKey(m_window->GetWindow(), GLFW_KEY_A) == GLFW_PRESS;
        break;
    case Key::KEY_D:
        return glfwGetKey(m_window->GetWindow(), GLFW_KEY_D) == GLFW_PRESS;
        break;
    case Key::KEY_LCTRL:
        return glfwGetKey(m_window->GetWindow(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
        break;
    case Key::KEY_LSHIFT:
        return glfwGetKey(m_window->GetWindow(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
        break;
    default:
        return false;
        break;
    }
}

int InputSystem::GetMouseButtonPress(MouseButton button) const {
    switch (button) {
    case Horizon::InputSystem::MouseButton::LEFT_BUTTON:
        return glfwGetMouseButton(m_window->GetWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        break;
    case Horizon::InputSystem::MouseButton::RIGHT_BUTTON:
        return glfwGetMouseButton(m_window->GetWindow(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        break;
    default:
        return 0;
        break;
    }
}

int InputSystem::GetMouseButtonRelease(MouseButton button) const {
    switch (button) {
    case Horizon::InputSystem::MouseButton::LEFT_BUTTON:
        return glfwGetMouseButton(m_window->GetWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE;
        break;
    case Horizon::InputSystem::MouseButton::RIGHT_BUTTON:
        return glfwGetMouseButton(m_window->GetWindow(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE;
        break;
    default:
        return 0;
        break;
    }
}

} // namespace Horizon
