#pragma once

#include "config.h"

class AntialiasingData {
  public:
    AntialiasingData(Backend::RHI *rhi) noexcept;
    ~AntialiasingData() noexcept = default;

    const Math::float2 &GetJitterOffset() noexcept;

    Shader *taa_cs;
    Pipeline *taa_pass;

    Texture* previous_color_texture;
    Texture* output_color_texture;

    static constexpr u32 TAA_SAMPLE_COUNT = 16;

    Container::FixedArray<Math::float2, TAA_SAMPLE_COUNT> taa_samples;
    u32 taa_sample_index = 0;
};
