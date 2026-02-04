#pragma once

#include "header.h"
#include <scene/scene_manager/scene_manager.h>
class SceneData
{
  public:
    SceneData(SceneManager *scene_manager) noexcept;
    ~SceneData() noexcept = default;
    Horizon::SceneManager *m_scene_manager{};

    Camera *scene_camera;
    CameraController *scene_camera_controller;
};
