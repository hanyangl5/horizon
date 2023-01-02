#pragma once

#include "config.h"

class ShadowMapData {
  public:
    ShadowMapData(RHI *rhi) noexcept;
    ~ShadowMapData() noexcept;

    Shader *shadow_map_vs, *shadow_map_ps;
    Pipeline *shadow_map_pass;

    // buffer/texture/rt resources

    RenderTarget *depth;
};

class ConatactShadowData {
  public:
    ConatactShadowData(RHI *rhi) noexcept;
    ~ConatactShadowData() noexcept;

    Shader *contact_shadow_cs;
    Pipeline *contact_shadow_pass;
};