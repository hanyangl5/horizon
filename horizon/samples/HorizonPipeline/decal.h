#pragma once

#include "config.h"

class DecalData {
  public:
    DecalData(RHI* rhi, ResourceManager* resource_manager) noexcept;
    ~DecalData() noexcept = default;

    Shader *decal_vs;
    Shader *decal_ps;
    Pipeline *decal_pass;

    // buffer/texture/rt resources

    struct DecalConstant {} decal_constant;
    Buffer *decal_buffer{};
    Texture *decal_texture;
};