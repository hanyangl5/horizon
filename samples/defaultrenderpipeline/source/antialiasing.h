#pragma once

#include "header.h"

class AntialiasingPass {
  public:
    explicit AntialiasingPass(Backend::RHI *rhi) noexcept;
    ~AntialiasingPass() noexcept;

    const Math::float2 &GetJitterOffset() noexcept;

    Backend::RHI *mRhi;
    Shader *taa_cs;
    Pipeline *taa_pass;

    Texture *previous_color_texture;
    Texture *output_color_texture;

    static constexpr u32 TAA_SAMPLE_COUNT = 16;

    std::array<Math::float2, TAA_SAMPLE_COUNT> taa_samples;
    u32 taa_sample_index = 0;

    struct TAAPrevCurrOffset {
        Math::float2 prev_offset;
        Math::float2 curr_offset;
    } taa_prev_curr_offset;

    Buffer *taa_prev_curr_offset_buffer;
};
