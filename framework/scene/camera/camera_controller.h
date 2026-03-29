/*****************************************************************/ /**
                                                                     * \file   CameraController.h
                                                                     * \brief
                                                                     *
                                                                     * \author hylu
                                                                     * \date   November 2022
                                                                     *********************************************************************/

#pragma once

#include <core/math.h>
#include <core/window.h>
#include <scene/camera/camera.h>

namespace Horizon
{

class CameraController
{
  public:
    CameraController(Camera *camera) noexcept;

    ~CameraController() noexcept;

    CameraController(const CameraController &rhs) noexcept = delete;

    CameraController &operator=(const CameraController &rhs) noexcept = delete;

    CameraController(CameraController &&rhs) noexcept = delete;

    CameraController &operator=(CameraController &&rhs) noexcept = delete;

  public:
    void ProcessInput(Window *window);

  private:
    Camera *m_camera = nullptr;
};

} // namespace Horizon
