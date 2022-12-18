#pragma once

#include "config.h"

class DecalData {
  public:
    DecalData(Backend::RHI *rhi, SceneManager* scene_manager) noexcept;
    ~DecalData() noexcept = default;

    SceneManager *m_scene_manager;

    Shader *decal_ps;
    Shader *decal_vs;

    Pipeline *decal_pass;
};
