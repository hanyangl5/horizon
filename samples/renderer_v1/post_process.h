#pragma once

#include "header.h"

class AutoExposure {
  public:
    explicit AutoExposure(Backend::RHI *rhi) noexcept;
    ~AutoExposure() noexcept;

    Backend::RHI *mRhi;

    Shader *luminance_histogram_cs;
    Pipeline *luminance_histogram_pass;

    Shader *luminance_average_cs;
    Pipeline *luminance_average_pass;

    struct LumincaneHistogramConstants {
        u32 width;
        u32 height;
        u32 pixelCount;
        //float minLogLuminance;
        //float logLuminanceRange;
        //float timeDelta;
        //float tau;
        float maxLuminance;
        float timeCoeff;
    } luminance_histogram_constants;

    Buffer *luminance_histogram_constants_buffer;
    Buffer *histogram_buffer;
    Buffer *adapted_muminance_buffer;
};

class PostProcessData {
  public:
    explicit PostProcessData(Backend::RHI *rhi) noexcept;
    ~PostProcessData() noexcept;
    Backend::RHI *mRhi;

    std::unique_ptr<AutoExposure> auto_exposure_pass;

    Shader *post_process_cs;
    Pipeline *post_process_pass;

    struct ExposureConstant {
        Math::float4 exposure_ev100__;
    } exposure_constants;

    Texture *pp_color_image;
    Buffer *exposure_constants_buffer;
};
