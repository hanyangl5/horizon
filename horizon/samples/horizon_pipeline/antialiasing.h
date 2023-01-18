#pragma once

#include "config.h"

class TAAData {
  public:
    TAAData(Backend::RHI *rhi) noexcept;
    ~TAAData() noexcept = default;

    const math::Vector2f &GetJitterOffset() noexcept;

    Shader *taa_cs;
    Pipeline *taa_pass;

    Texture* previous_color_texture;
    Texture* output_color_texture;

    static constexpr u32 TAA_SAMPLE_COUNT = 16;

    Container::FixedArray<math::Vector2f, TAA_SAMPLE_COUNT> taa_samples;
    u32 taa_sample_index = 0;

    struct TAAPrevCurrOffset {
        math::Vector2f prev_offset;
        math::Vector2f curr_offset;
    } taa_prev_curr_offset;

    Buffer* taa_prev_curr_offset_buffer;
};
