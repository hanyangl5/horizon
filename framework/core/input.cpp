/*****************************************************************/ /**
                                                                     * \file   WindowInput.cpp
                                                                     * \brief
                                                                     *
                                                                     * \author hylu
                                                                     * \date   November 2022
                                                                     *********************************************************************/

#include "input.h"

namespace Horizon::Input
{

f32 last_x;
f32 last_y;
bool first_mouse = true;

Direction ProcessKeyboardInput(Window *window)
{
    if (GetKeyPress(window, Key::ESCAPE))
    {
        window->Close();
    }
    if (GetKeyPress(window, Key::KEY_W))
    {
        return Direction::BACKWARD;
    }
    if (GetKeyPress(window, Key::KEY_S))
    {
        return Direction::FORWARD;
    }
    if (GetKeyPress(window, Key::KEY_A))
    {
        return Direction::LEFT;
    }
    if (GetKeyPress(window, Key::KEY_D))
    {
        return Direction::RIGHT;
    }
    if (GetKeyPress(window, Key::SPACE))
    {
        return Direction::UP;
    }
    if (GetKeyPress(window, Key::KEY_LCTRL))
    {
        return Direction::DOWN;
    }
    return {};
}

Math::float2 ProcessMouseInput(Window *window)
{
#ifdef __ANDROID__
    (void)window;
    return {};
#else
    if (window == nullptr || window->GetSDLWindow() == nullptr)
    {
        return {};
    }

    if (SDL_GetMouseFocus() != window->GetSDLWindow())
    {
        first_mouse = true;
        return {};
    }

    float xposIn = 0.0f;
    float yposIn = 0.0f;
    SDL_GetMouseState(&xposIn, &yposIn);

    f32 xpos = static_cast<f32>(xposIn);
    f32 ypos = static_cast<f32>(yposIn);

    if (first_mouse)
    {

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

    if (GetMouseButtonPress(window, MouseButton::RIGHT_BUTTON))
    {
        return Math::float2{xoffset, yoffset};
    }
    else if (GetMouseButtonRelease(window, MouseButton::RIGHT_BUTTON))
    {
        first_mouse = true;
    }
    return {};
#endif
}

bool GetKeyPress(Window *window, Key inputKey)
{
#ifdef __ANDROID__
    (void)window;
    (void)inputKey;
    return false;
#else
    if (window == nullptr || window->GetSDLWindow() == nullptr)
    {
        return false;
    }

    if (SDL_GetKeyboardFocus() != window->GetSDLWindow())
    {
        return false;
    }

    const bool *keyboard_state = SDL_GetKeyboardState(nullptr);
    if (keyboard_state == nullptr)
    {
        return false;
    }

    switch (inputKey)
    {
    case Key::ESCAPE:
        return keyboard_state[SDL_SCANCODE_ESCAPE];
    case Key::SPACE:
        return keyboard_state[SDL_SCANCODE_SPACE];
    case Key::KEY_W:
        return keyboard_state[SDL_SCANCODE_W];
    case Key::KEY_S:
        return keyboard_state[SDL_SCANCODE_S];
    case Key::KEY_A:
        return keyboard_state[SDL_SCANCODE_A];
    case Key::KEY_D:
        return keyboard_state[SDL_SCANCODE_D];
    case Key::KEY_LCTRL:
        return keyboard_state[SDL_SCANCODE_LCTRL];
    case Key::KEY_LSHIFT:
        return keyboard_state[SDL_SCANCODE_LSHIFT];
    case Key::KEY_1:
        return keyboard_state[SDL_SCANCODE_1];
    case Key::KEY_2:
        return keyboard_state[SDL_SCANCODE_2];
    case Key::KEY_3:
        return keyboard_state[SDL_SCANCODE_3];
    case Key::KEY_4:
        return keyboard_state[SDL_SCANCODE_4];
    default:
        break;
    }
    return false;
#endif
}

int GetMouseButtonPress(Window *window, MouseButton button)
{
#ifdef __ANDROID__
    (void)window;
    (void)button;
    return 0;
#else
    if (window == nullptr || window->GetSDLWindow() == nullptr)
    {
        return 0;
    }

    if (SDL_GetMouseFocus() != window->GetSDLWindow())
    {
        return 0;
    }

    const SDL_MouseButtonFlags mouse_state = SDL_GetMouseState(nullptr, nullptr);
    switch (button)
    {
    case MouseButton::LEFT_BUTTON:
        return (mouse_state & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
    case MouseButton::RIGHT_BUTTON:
        return (mouse_state & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) != 0;
    default:
        break;
    }
    return 0;
#endif
}

int GetMouseButtonRelease(Window *window, MouseButton button)
{
#ifdef __ANDROID__
    (void)window;
    (void)button;
    return 0;
#else
    if (window == nullptr || window->GetSDLWindow() == nullptr)
    {
        return 0;
    }

    if (SDL_GetMouseFocus() != window->GetSDLWindow())
    {
        return 0;
    }

    const SDL_MouseButtonFlags mouse_state = SDL_GetMouseState(nullptr, nullptr);
    switch (button)
    {
    case MouseButton::LEFT_BUTTON:
        return (mouse_state & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) == 0;
    case MouseButton::RIGHT_BUTTON:
        return (mouse_state & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) == 0;
    default:
        break;
    }
    return 0;
#endif
}

} // namespace Horizon::Input
