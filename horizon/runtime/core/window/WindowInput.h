/*****************************************************************//**
 * \file   WindowInput.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#include "Window.h"

#include "../math/Math.h"

namespace Horizon::Input {

enum class Key { ESCAPE, SPACE, KEY_W, KEY_S, KEY_A, KEY_D, KEY_LCTRL, KEY_LSHIFT };

enum class MouseButton { LEFT_BUTTON, RIGHT_BUTTON };

enum class INPUT_STATE { PRESS, RELEASE };

enum class Direction { INVALID, FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };

extern f32 last_x;
extern f32 last_y;
extern bool first_mouse;

Direction ProcessKeyboardInput(Window *window);

Math::float2 ProcessMouseInput(Window *window);

bool GetKeyPress(Window *window, Key inputKey);

int GetMouseButtonPress(Window *window, MouseButton button);

int GetMouseButtonRelease(Window *window, MouseButton button);
//
//bool Input::IsKeyPressed(const KeyCode key) {
//    auto *window = static_cast<GLFWwindow *>(Application::Get().GetWindow().GetNativeWindow());
//    auto state = glfwGetKey(window, static_cast<int32_t>(key));
//    return state == GLFW_PRESS;
//}
//
//bool Input::IsMouseButtonPressed(const MouseCode button) {
//    auto *window = static_cast<GLFWwindow *>(Application::Get().GetWindow().GetNativeWindow());
//    auto state = glfwGetMouseButton(window, static_cast<int32_t>(button));
//    return state == GLFW_PRESS;
//}
//
//glm::vec2 Input::GetMousePosition() {
//    auto *window = static_cast<GLFWwindow *>(Application::Get().GetWindow().GetNativeWindow());
//    double xpos, ypos;
//    glfwGetCursorPos(window, &xpos, &ypos);
//
//    return {(float)xpos, (float)ypos};
//}
//
//float Input::GetMouseX() { return GetMousePosition().x; }
//
//float Input::GetMouseY() { return GetMousePosition().y; }
} // namespace Horizon