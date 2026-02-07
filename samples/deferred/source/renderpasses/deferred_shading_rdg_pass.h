#pragma once

#include "../header.h"
#include "../scene.h"
#include <render_graph/frame_graph.h>

class DeferredShadingRDGPass : public Horizon::Backend::RDGPass
{
  public:
    // Constants
    struct DeferredShadingConstants
    {
        Math::float4x4 inverse_vp;
        Math::float4 camera_pos;
        u32 width;
        u32 height;
        float ibl_intensity, pad1;
    };
    struct DiffuseIrradianceSH3
    {
        std::array<Math::float4, 9> sh;
    };

  public:
    DeferredShadingRDGPass(RHI *rhi, Horizon::SceneManager *scene_manager);
    ~DeferredShadingRDGPass();

    void ImportResources(Horizon::Backend::FrameGraph *frame_graph) override;
    void Setup(Horizon::Backend::FrameGraphBuilder &builder) override;
    void Execute(CommandList *command_list, Horizon::Backend::FrameGraphBuilder &builder) override;

    // Set resource handles from other passes (called before Setup)
    void SetGBufferHandles(Horizon::Backend::TextureHandle gbuffer0, Horizon::Backend::TextureHandle gbuffer1,
                           Horizon::Backend::TextureHandle gbuffer2, Horizon::Backend::TextureHandle gbuffer3,
                           Horizon::Backend::TextureHandle depth);
    void SetSSAOBlurHandle(Horizon::Backend::TextureHandle ssao_blur);

    // Accessors for ResourceUploadPass
    Buffer *GetDeferredShadingConstantsBuffer() const { return m_deferred_shading_constants_buffer; }
    Buffer *GetDiffuseIrradianceSH3Buffer() const { return m_diffuse_irradiance_sh3_buffer; }
    DeferredShadingConstants &GetDeferredShadingConstants()
    {
        return m_deferred_shading_constants;
    }
    DiffuseIrradianceSH3 &GetDiffuseIrradianceSH3Constants() { return m_diffuse_irradiance_sh3_constants; }
    Texture *GetPrefilteredIrradianceEnvMap() const { return m_prefiltered_irradiance_env_map; }
    Texture *GetBRDFLUT() const { return m_brdf_lut; }
    TextureDataDesc &GetPrefilteredIrradianceEnvMapData() { return m_prefilered_irradiance_env_map_data; }
    TextureDataDesc &GetBRDFLUTDataDesc() { return m_brdf_lut_data_desc; }
    Horizon::Backend::TextureHandle GetShadingColorHandle() const { return m_shading_color_handle; }
    Horizon::Backend::TextureHandle GetBRDFLUTHandle() const { return m_brdf_lut_handle; }
    Horizon::Backend::TextureHandle GetPrefilteredEnvHandle() const { return m_prefiltered_env_handle; }

  public:

  private:
    RHI *m_rhi;
    Horizon::SceneManager *m_scene_manager;

    // Pipeline resources (owned by pass)
    Shader *m_shading_cs;
    Pipeline *m_shading_pipeline;

    DeferredShadingConstants m_deferred_shading_constants;
    Buffer *m_deferred_shading_constants_buffer;

    // IBL resources
    DiffuseIrradianceSH3 m_diffuse_irradiance_sh3_constants;
    Buffer *m_diffuse_irradiance_sh3_buffer;
    Texture *m_prefiltered_irradiance_env_map;
    Texture *m_brdf_lut;
    Sampler *m_ibl_sampler;
    TextureDataDesc m_prefilered_irradiance_env_map_data;
    TextureDataDesc m_brdf_lut_data_desc;
    
    // Shading color texture (owned by pass)
    Texture *m_shading_color_texture;

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
