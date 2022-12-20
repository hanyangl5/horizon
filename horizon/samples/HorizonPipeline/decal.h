#pragma once

#include "config.h"
#include "deferred.h"
class DecalData {
  public:
    DecalData(Backend::RHI *rhi, SceneManager *scene_manager, DeferredData *deferred_data) noexcept;
    ~DecalData() noexcept = default;

    SceneManager *m_scene_manager;

    Shader *decal_ps;
    Shader *decal_vs;

    Pipeline *decal_pass;
};
