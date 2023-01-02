/*****************************************************************//**
 * \file   WindowInput.cpp
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#include "window_input.h"

namespace Horizon::Input {

static f32 last_x;
static f32 last_y;
static bool first_mouse = true;

Direction ProcessKeyboardInput(Window *window) {
    if (GetKeyPress(window, Key::ESCAPE)) {
        window->Close();
    }
    if (GetKeyPress(window, Key::KEY_W)) {
        return Direction::FORWARD;
    }
    if (GetKeyPress(window, Key::KEY_S)) {
        return Direction::BACKWARD;
    }
    if (GetKeyPress(window, Key::KEY_A)) {
        return Direction::LEFT;
    }
    if (GetKeyPress(window, Key::KEY_D)) {
        return Direction::RIGHT;
    }
    if (GetKeyPress(window, Key::SPACE)) {
        return Direction::UP;
    }
    if (GetKeyPress(window, Key::KEY_LCTRL)) {
        return Direction::DOWN;
    }
    return {};
}

Math::float2 ProcessMouseInput(Window *window) {
    f64 xposIn, yposIn;
    glfwGetCursorPos(window->GetWindow(), &xposIn, &yposIn);

    f32 xpos = static_cast<f32>(xposIn);
    f32 ypos = static_cast<f32>(yposIn);

    if (first_mouse) {

        last_x = window->GetWidth() / 2.0f;
        last_y = window->GetHeight() / 2.0f;
        first_mouse = true;
        last_x = xpos;
        last_y = ypos;
        first_mouse = false;
    }

    f32 xoffset = xpos - last_x;
    f32 yoffset = ypos - last_y; // reversed since y-coordinates go from bottom to top

    last_x = xpos;
    last_y = ypos;

    if (GetMouseButtonPress(window, MouseButton::RIGHT_BUTTON)) {
        return Math::float2{xoffset, yoffset};
    } else if (GetMouseButtonRelease(window, MouseButton::RIGHT_BUTTON)) {
        first_mouse = true;
    }
    return {};
}

bool GetKeyPress(Window *window, Key inputKey) {
    switch (inputKey) {
    case Key::ESCAPE:
        return glfwGetKey(window->GetWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS;
        break;
    case Key::SPACE:
        return glfwGetKey(window->GetWindow(), GLFW_KEY_SPACE) == GLFW_PRESS;
        break;
    case Key::KEY_W:
        return glfwGetKey(window->GetWindow(), GLFW_KEY_W) == GLFW_PRESS;
        break;
    case Key::KEY_S:
        return glfwGetKey(window->GetWindow(), GLFW_KEY_S) == GLFW_PRESS;
        break;
    case Key::KEY_A:
        return glfwGetKey(window->GetWindow(), GLFW_KEY_A) == GLFW_PRESS;
        break;
    case Key::KEY_D:
        return glfwGetKey(window->GetWindow(), GLFW_KEY_D) == GLFW_PRESS;
        break;
    case Key::KEY_LCTRL:
        return glfwGetKey(window->GetWindow(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
        break;
    case Key::KEY_LSHIFT:
        return glfwGetKey(window->GetWindow(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
        break;
    default:
        return false;
        break;
    }
}

int GetMouseButtonPress(Window *window, MouseButton button) {
    switch (button) {
    case MouseButton::LEFT_BUTTON:
        return glfwGetMouseButton(window->GetWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        break;
    case MouseButton::RIGHT_BUTTON:
        return glfwGetMouseButton(window->GetWindow(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        break;
    default:
        return 0;
        break;
    }
}

int GetMouseButtonRelease(Window *window, MouseButton button) {
    switch (button) {
    case MouseButton::LEFT_BUTTON:
        return glfwGetMouseButton(window->GetWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE;
        break;
    case MouseButton::RIGHT_BUTTON:
        return glfwGetMouseButton(window->GetWindow(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE;
        break;
    default:
        return 0;
        break;
    }
}

} // namespace Horizon::Input
