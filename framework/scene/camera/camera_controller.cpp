/*****************************************************************/ /**
                                                                     * \file   CameraController.cpp
                                                                     * \brief
                                                                     *
                                                                     * \author hylu
                                                                     * \date   November 2022
                                                                     *********************************************************************/

#include "camera_controller.h"

#include <cassert>

#ifndef __ANDROID__
#include <core/input.h>
#endif

namespace Horizon
{

CameraController::CameraController(Camera *camera) noexcept : m_camera(camera)
{
}

CameraController::~CameraController() noexcept
{
}

void CameraController::ProcessInput(Window *window)
{
    assert(m_camera != nullptr);
    assert(m_camera->GetCameraSpeed() != 0);

#ifndef __ANDROID__
    auto direction = Input::ProcessKeyboardInput(window);
    m_camera->Move(direction);
    auto rotation = Input::ProcessMouseInput(window);
    m_camera->Rotate(rotation.x, rotation.y);
#endif
    m_camera->UpdateViewMatrix();
}

} // namespace Horizon
