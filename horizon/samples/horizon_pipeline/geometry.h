#pragma once

#include "config.h"

class GeometryData {
  public:
    GeometryData(RHI* rhi) noexcept;
    ~GeometryData() noexcept;

    // pass resources
    GraphicsPipelineCreateInfo graphics_pass_ci{};

    Shader *geometry_vs, *geometry_ps;
    Pipeline *geometry_pass;

    // buffer/texture/rt resources

    RenderTarget* gbuffer0;
    RenderTarget* gbuffer1;
    RenderTarget* gbuffer2;
    RenderTarget* gbuffer3;
    RenderTarget* gbuffer4;
    //RenderTarget* vbuffer0;

    RenderTarget* depth;

};