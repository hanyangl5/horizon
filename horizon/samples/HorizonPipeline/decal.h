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

    Pipeline *decal_pass{};

    Texture *scene_depth_texture{};

    struct DecalConstants {
        Math::float4x4 inverse_vp;
        u32 resolution[2];
        u32 pad[2];
    } decal_constants;

    Buffer *decal_constants_buffer{};
};
