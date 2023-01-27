/*****************************************************************//**
 * \file   window_input.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

// standard libraries

// third party libraries

// project headers
#include <runtime/core/math/math.h>
#include <runtime/core/window/window.h>

namespace Horizon::Input {

enum class Key { ESCAPE, SPACE, KEY_W, KEY_S, KEY_A, KEY_D, KEY_LCTRL, KEY_LSHIFT };

enum class MouseButton { LEFT_BUTTON, RIGHT_BUTTON };

enum class INPUT_STATE { PRESS, RELEASE };

enum class Direction { INVALID, FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };

Direction ProcessKeyboardInput(Window *window);

math::Vector2f ProcessMouseInput(Window *window);

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