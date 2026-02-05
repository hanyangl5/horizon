#pragma once
#include "../header.h"
#include <render_graph/frame_graph.h>

class AutoExposure
{
  public:
    explicit AutoExposure(Horizon::Backend::RHI *rhi) noexcept;
    ~AutoExposure() noexcept;

     Horizon::Backend::RHI *mRhi;

    Horizon::Shader *luminance_histogram_cs;
    Horizon::Pipeline *luminance_histogram_pass;

    Shader *luminance_average_cs;
    Pipeline *luminance_average_pass;

    struct LuminanceHistogramConstants
    {
        u32 width;
        u32 height;
        u32 pixelCount;
        // float minLogLuminance;
        // float logLuminanceRange;
        // float timeDelta;
        // float tau;
        float maxLuminance;
        float timeCoeff;
    } luminance_histogram_constants;

    Buffer *luminance_histogram_constants_buffer;
    Buffer *histogram_buffer;
    Buffer *adapted_muminance_buffer;

    // RDG methods
    void ImportResources(Horizon::Backend::FrameGraph *frame_graph,
                         Horizon::Backend::BufferHandle &histogram_buffer_handle,
                         Horizon::Backend::BufferHandle &adapted_luminance_handle);

    void SetupLuminanceHistogramPass(Horizon::Backend::FrameGraphBuilder &builder,
                                      Horizon::Backend::TextureHandle shading_color_handle,
                                      Horizon::Backend::BufferHandle histogram_buffer_handle,
                                      Horizon::Backend::BufferHandle adapted_luminance_handle);

    void ExecuteLuminanceHistogramPass(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder,
                                        Horizon::Backend::TextureHandle shading_color_handle,
                                        Horizon::Backend::BufferHandle histogram_buffer_handle,
                                        Horizon::Backend::BufferHandle adapted_luminance_handle);

    void SetupLuminanceAveragePass(Horizon::Backend::FrameGraphBuilder &builder,
                                    Horizon::Backend::BufferHandle histogram_buffer_handle,
                                    Horizon::Backend::BufferHandle adapted_luminance_handle);

    void ExecuteLuminanceAveragePass(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder,
                                     Horizon::Backend::BufferHandle histogram_buffer_handle,
                                     Horizon::Backend::BufferHandle adapted_luminance_handle);
};

class PostProcessingPass
{
  public:
    explicit PostProcessingPass(Backend::RHI *rhi) noexcept;
    ~PostProcessingPass() noexcept;
    Backend::RHI *mRhi;

    std::unique_ptr<AutoExposure> auto_exposure_pass;

    Shader *post_process_cs;
    Pipeline *post_process_pass;

    struct ExposureConstant
    {
        Math::float4 exposure_ev100__;
    } exposure_constants;

    Texture *pp_color_image;
    Buffer *exposure_constants_buffer;

    // RDG methods
    void ImportResources(Horizon::Backend::FrameGraph *frame_graph, Horizon::Backend::TextureHandle &pp_color_handle);

    void SetupPostProcessPass(Horizon::Backend::FrameGraphBuilder &builder,
                              Horizon::Backend::TextureHandle shading_color_handle,
                              Horizon::Backend::TextureHandle pp_color_handle,
                              Horizon::Backend::BufferHandle adapted_luminance_handle);

    void ExecutePostProcessPass(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder,
                                Horizon::Backend::TextureHandle shading_color_handle,
                                Horizon::Backend::TextureHandle pp_color_handle,
                                Horizon::Backend::BufferHandle adapted_luminance_handle);
};
