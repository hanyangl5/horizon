#pragma once

#include "header.h"
#include "scene.h"
#include <render_graph/frame_graph.h>

class DeferredShadingRDGPass : public Horizon::Backend::RDGPass
{
  public:
    DeferredShadingRDGPass(RHI *rhi, Horizon::SceneManager *scene_manager);
    ~DeferredShadingRDGPass() override = default;

    void ImportResources(Horizon::Backend::FrameGraph *frame_graph) override;
    void Setup(Horizon::Backend::FrameGraphBuilder &builder) override;
    void Execute(CommandList *command_list, Horizon::Backend::FrameGraphBuilder &builder) override;

    // Set resource handles from other passes (called before Setup)
    void SetGBufferHandles(Horizon::Backend::TextureHandle gbuffer0, Horizon::Backend::TextureHandle gbuffer1,
                           Horizon::Backend::TextureHandle gbuffer2, Horizon::Backend::TextureHandle gbuffer3,
                           Horizon::Backend::TextureHandle depth);
    void SetSSAOBlurHandle(Horizon::Backend::TextureHandle ssao_blur);

  private:
    RHI *m_rhi;
    Horizon::SceneManager *m_scene_manager;

    // Pipeline resources (owned by pass)
    Shader *m_shading_cs;
    Pipeline *m_shading_pipeline;

    // Constants
    struct DeferredShadingConstants
    {
        Math::float4x4 inverse_vp;
        Math::float4 camera_pos;
        u32 width;
        u32 height;
        float ibl_intensity, pad1;
    } m_deferred_shading_constants;
    Buffer *m_deferred_shading_constants_buffer;

    // IBL resources
    struct DiffuseIrradianceSH3
    {
        std::array<Math::float4, 9> sh;
    } m_diffuse_irradiance_sh3_constants;
    Buffer *m_diffuse_irradiance_sh3_buffer;
    Texture *m_prefiltered_irradiance_env_map;
    Texture *m_brdf_lut;
    Sampler *m_ibl_sampler;

    // Resource handles (managed by FrameGraph)
    Horizon::Backend::TextureHandle m_gbuffer0_handle;
    Horizon::Backend::TextureHandle m_gbuffer1_handle;
    Horizon::Backend::TextureHandle m_gbuffer2_handle;
    Horizon::Backend::TextureHandle m_gbuffer3_handle;
    Horizon::Backend::TextureHandle m_depth_handle;
    Horizon::Backend::TextureHandle m_shading_color_handle;
    Horizon::Backend::TextureHandle m_ssao_blur_handle;
    Horizon::Backend::TextureHandle m_brdf_lut_handle;
    Horizon::Backend::TextureHandle m_prefiltered_env_handle;
};
