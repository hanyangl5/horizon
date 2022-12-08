#pragma once

#include "config.h"

class DecalData {
  public:
    DecalData(Backend::RHI *rhi) noexcept;
    ~DecalData() noexcept = default;

    Shader *decal_cs;
    Pipeline *decal_pass;
};
