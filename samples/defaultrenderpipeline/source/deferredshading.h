#pragma once

#include "header.h"
#include "scene.h"
#include <render_graph/frame_graph.h>


class DeferredShadingPass
{
  public:
    explicit DeferredShadingPass(RHI *rhi) noexcept;
    ~DeferredShadingPass() noexcept;

    Backend::RHI *mRhi;

    // pass resources

    GraphicsPipelineCreateInfo graphics_pass_ci{};

    Shader *geometry_vs, *geometry_ps;
    Pipeline *geometry_pass;

    Shader *shading_cs;
    Pipeline *shading_pass;

    // buffer/texture/rt resources

    struct DeferredShadingConstants
    {
        Math::float4x4 inverse_vp;
        Math::float4 camera_pos;
        u32 width;
        u32 height;
        float ibl_intensity, pad1;
    } deferred_shading_constants;
    Buffer *deferred_shading_constants_buffer;

    RenderTarget *gbuffer0;
    RenderTarget *gbuffer1;
    RenderTarget *gbuffer2;
    RenderTarget *gbuffer3;
    RenderTarget *gbuffer4;

    RenderTarget *depth;

    Texture *shading_color_image;

    // ibl
    struct DiffuseIrradianceSH3
    {
        std::array<Math::float4, 9> sh;
    } diffuse_irradiance_sh3_constants;
    Buffer *diffuse_irradiance_sh3_buffer;
    TextureDataDesc prefilered_irradiance_env_map_data;
    Texture *prefiltered_irradiance_env_map;
    TextureDataDesc brdf_lut_data_desc;
    Texture *brdf_lut;

    Sampler *ibl_sampler{};

    // RDG methods
    void ImportResources(Horizon::Backend::FrameGraph *frame_graph, Horizon::Backend::TextureHandle &gbuffer0_handle,
                         Horizon::Backend::TextureHandle &gbuffer1_handle,
                         Horizon::Backend::TextureHandle &gbuffer2_handle,
                         Horizon::Backend::TextureHandle &gbuffer3_handle,
                         Horizon::Backend::TextureHandle &gbuffer4_handle,
                         Horizon::Backend::TextureHandle &depth_handle,
                         Horizon::Backend::TextureHandle &shading_color_handle,
                         Horizon::Backend::RenderTargetHandle &gbuffer0_rt_handle,
                         Horizon::Backend::RenderTargetHandle &gbuffer1_rt_handle,
                         Horizon::Backend::RenderTargetHandle &gbuffer2_rt_handle,
                         Horizon::Backend::RenderTargetHandle &gbuffer3_rt_handle,
                         Horizon::Backend::RenderTargetHandle &gbuffer4_rt_handle,
                         Horizon::Backend::RenderTargetHandle &depth_rt_handle,
                         Horizon::Backend::TextureHandle &brdf_lut_handle,
                         Horizon::Backend::TextureHandle &prefiltered_env_handle);

    void SetupGeometryPass(Horizon::Backend::FrameGraphBuilder &builder,
                           Horizon::Backend::RenderTargetHandle gbuffer0_rt_handle,
                           Horizon::Backend::RenderTargetHandle gbuffer1_rt_handle,
                           Horizon::Backend::RenderTargetHandle gbuffer2_rt_handle,
                           Horizon::Backend::RenderTargetHandle gbuffer3_rt_handle,
                           Horizon::Backend::RenderTargetHandle gbuffer4_rt_handle,
                           Horizon::Backend::RenderTargetHandle depth_rt_handle,
                           Horizon::Backend::TextureHandle gbuffer0_handle,
                           Horizon::Backend::TextureHandle gbuffer1_handle,
                           Horizon::Backend::TextureHandle gbuffer2_handle,
                           Horizon::Backend::TextureHandle gbuffer3_handle,
                           Horizon::Backend::TextureHandle gbuffer4_handle,
                           Horizon::Backend::TextureHandle depth_handle);

    void ExecuteGeometryPass(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder,
                             Horizon::Backend::RenderTargetHandle gbuffer0_rt_handle,
                             Horizon::Backend::RenderTargetHandle gbuffer1_rt_handle,
                             Horizon::Backend::RenderTargetHandle gbuffer2_rt_handle,
                             Horizon::Backend::RenderTargetHandle gbuffer3_rt_handle,
                             Horizon::Backend::RenderTargetHandle gbuffer4_rt_handle,
                             Horizon::Backend::RenderTargetHandle depth_rt_handle, Horizon::SceneManager *scene_manager,
                             Sampler *sampler, Buffer *taa_prev_curr_offset_buffer);

    void SetupDeferredShadingPass(Horizon::Backend::FrameGraphBuilder &builder,
                                   Horizon::Backend::TextureHandle gbuffer0_handle,
                                   Horizon::Backend::TextureHandle gbuffer1_handle,
                                   Horizon::Backend::TextureHandle gbuffer2_handle,
                                   Horizon::Backend::TextureHandle gbuffer3_handle,
                                   Horizon::Backend::TextureHandle depth_handle,
                                   Horizon::Backend::TextureHandle shading_color_handle,
                                   Horizon::Backend::TextureHandle ssao_blur_handle,
                                   Horizon::Backend::TextureHandle brdf_lut_handle,
                                   Horizon::Backend::TextureHandle prefiltered_env_handle);

    void ExecuteDeferredShadingPass(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder,
                                    Horizon::Backend::TextureHandle gbuffer0_handle,
                                    Horizon::Backend::TextureHandle gbuffer1_handle,
                                    Horizon::Backend::TextureHandle gbuffer2_handle,
                                    Horizon::Backend::TextureHandle gbuffer3_handle,
                                    Horizon::Backend::TextureHandle depth_handle,
                                    Horizon::Backend::TextureHandle shading_color_handle,
                                    Horizon::Backend::TextureHandle ssao_blur_handle,
                                    Horizon::Backend::TextureHandle brdf_lut_handle,
                                    Horizon::Backend::TextureHandle prefiltered_env_handle, Horizon::SceneManager *scene_manager);
};