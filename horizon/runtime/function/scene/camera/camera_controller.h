/*****************************************************************//**
 * \file   camera_controller.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#include <runtime/core/math/Math.h>
#include <runtime/core/window/Window.h>
#include <runtime/function/scene/camera/Camera.h>

namespace Horizon {

class CameraController {
  public:
    CameraController(Camera *camera) noexcept;

    ~CameraController() noexcept;

    CameraController(const CameraController &rhs) noexcept = delete;

    CameraController &operator=(const CameraController &rhs) noexcept = delete;

    CameraController(CameraController &&rhs) noexcept = delete;

    CameraController &operator=(CameraController &&rhs) noexcept = delete;

  public:
    void ProcessInput(Window* window);

  private:
    Window *m_window = nullptr;
    Camera *m_camera = nullptr;
};

} // namespace Horizon