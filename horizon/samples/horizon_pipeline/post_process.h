#pragma once

#include "config.h"

class PostProcessData {
  public:
    PostProcessData(Backend::RHI *rhi) noexcept;
    ~PostProcessData() noexcept = default;

    Shader *post_process_cs;
    Pipeline *post_process_pass;

    struct ExposureConstant {
        Math::float4 exposure_ev100__;
    } exposure_constants;

    Texture* pp_color_texture;
    Buffer* exposure_constants_buffer;
};