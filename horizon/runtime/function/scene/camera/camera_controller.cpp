/*****************************************************************//**
 * \file   CameraController.cpp
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#include "camera_controller.h"

// standard libraries

// third party libraries

// project headers

#include <runtime/core/window/window_input.h>

namespace Horizon {

CameraController::CameraController(Camera *camera) noexcept : m_camera(camera) {
}

CameraController::~CameraController() noexcept {
}

void CameraController::ProcessInput(Window* window) {
    assert(m_camera != nullptr);
    assert(m_camera->GetCameraSpeed() != 0);

    glfwPollEvents();
    
    auto direction = Input::ProcessKeyboardInput(window);
    m_camera->Move(direction);
    auto rotation = Input::ProcessMouseInput(window);
    m_camera->Rotate(rotation.x, rotation.y);
    m_camera->UpdateViewMatrix();
}

} // namespace Horizon
