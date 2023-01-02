#pragma once

#include "config.h"

class SceneData {
  public:
    SceneData(SceneManager *scene_manager, Backend::RHI *rhi) noexcept;
    ~SceneData() noexcept = default;
    SceneManager *m_scene_manager{};

    Camera *scene_camera;
    CameraController *scene_camera_controller;

};
