#pragma once

#include "header.h"

class PostProcessData {
  public:
    explicit PostProcessData(Backend::RHI *rhi) noexcept;
    ~PostProcessData() noexcept;
    Backend::RHI *mRhi;

    Shader *post_process_cs;
    Pipeline *post_process_pass;

    struct ExposureConstant {
        Math::float4 exposure_ev100__;
    } exposure_constants;

    Texture *pp_color_image;
    Buffer *exposure_constants_buffer;
};
