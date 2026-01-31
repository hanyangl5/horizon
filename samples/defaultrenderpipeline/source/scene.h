#pragma once

#include "header.h"

class SceneData {
  public:
    SceneData(SceneManager *scene_manager) noexcept;
    ~SceneData() noexcept = default;
    SceneManager *m_scene_manager{};

    Camera *scene_camera;
    CameraController *scene_camera_controller;
};
