#pragma once

#include "config.h"
#include "geometry.h"

class DecalData {
  public:
    DecalData(Backend::RHI *rhi, SceneManager *scene_manager, GeometryData *geometry_data) noexcept;
    ~DecalData() noexcept = default;

    SceneManager *m_scene_manager;

    Shader *decal_ps;
    Shader *decal_vs;

    Pipeline *decal_pass{};

    Texture *scene_depth_texture{};

};
