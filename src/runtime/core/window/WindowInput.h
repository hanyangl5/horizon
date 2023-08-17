/*****************************************************************/ /**
 * \file   WindowInput.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#include "Window.h"

#include <runtime/core/math/Math.h>

namespace Horizon::Input {

enum class Key {
    ESCAPE,
    SPACE,
    KEY_W,
    KEY_S,
    KEY_A,
    KEY_D,
    KEY_LCTRL,
    KEY_LSHIFT,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
};

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

} // namespace Horizon::Input