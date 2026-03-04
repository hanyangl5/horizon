#include "resource_upload_pass.h"
#include "deferred_geometry_rdg_pass.h"
#include "deferred_shading_rdg_pass.h"
#include "gtao_rdg_pass.h"
#include "post_process_rdg_pass.h"
#include "taa_rdg_pass.h"
#include <scene/scene_manager/scene_manager.h>

ResourceUploadRDGPass::ResourceUploadRDGPass(RHI *rhi, Horizon::SceneManager *scene_manager)
    : RDGPass("Resource Upload Pass", rhi), m_rhi(rhi), m_scene_manager(scene_manager)
{
}

ResourceUploadRDGPass::~ResourceUploadRDGPass()
{
}

void ResourceUploadRDGPass::ImportResources(Horizon::Backend::FrameGraph *frame_graph)
{
    // No resources to import - we just write to resources imported by other passes
}

void ResourceUploadRDGPass::SetResourceHandles(
    Horizon::Backend::TextureHandle shading_color, Horizon::Backend::TextureHandle pp_color,
    Horizon::Backend::TextureHandle gtao_factor, Horizon::Backend::TextureHandle gtao_blur,
    Horizon::Backend::TextureHandle output_color, Horizon::Backend::TextureHandle previous_color,
    Horizon::Backend::TextureHandle brdf_lut, Horizon::Backend::TextureHandle prefiltered_env)
{
    m_shading_color_handle = shading_color;
    m_pp_color_handle = pp_color;
    m_gtao_factor_handle = gtao_factor;
    m_gtao_blur_handle = gtao_blur;
    m_output_color_handle = output_color;
    m_previous_color_handle = previous_color;
    m_brdf_lut_handle = brdf_lut;
    m_prefiltered_env_handle = prefiltered_env;
}

void ResourceUploadRDGPass::SetPassPointers(DeferredShadingGeometryPass *geometry, DeferredShadingRDGPass *deferred,
                                            GTAORDGPass *gtao, PostProcessRDGPass *post_process, TAARDGPass *taa)
{
    m_geometry = geometry;
    m_deferred = deferred;
    m_gtao = gtao;
    m_post_process = post_process;
    m_taa = taa;
}

void ResourceUploadRDGPass::Setup(Horizon::Backend::FrameGraphBuilder &builder)
{
    builder.WriteTexture(m_shading_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteTexture(m_pp_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteTexture(m_gtao_factor_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteTexture(m_gtao_blur_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteTexture(m_output_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);

    if (m_first_frame)
    {
        builder.WriteTexture(m_previous_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
        builder.WriteTexture(m_brdf_lut_handle, ResourceState::RESOURCE_STATE_COPY_DEST);
        builder.WriteTexture(m_prefiltered_env_handle, ResourceState::RESOURCE_STATE_COPY_DEST);
    }
    else if (m_initialize_history)
    {
        builder.WriteTexture(m_previous_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    }
}

void ResourceUploadRDGPass::Execute(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder)
{
    (void)builder;
    // Upload textures, vertex/index buffer
    if (m_upload_scene_resources)
    {
        m_scene_manager->UploadBuiltInResources(cl);
        m_scene_manager->UploadMeshResources(cl);
    }
    // Scene data
    m_scene_manager->UpdateAnimationState();
    m_scene_manager->UploadAnimationResources(cl);
    m_scene_manager->UploadLightResources(cl);
    m_scene_manager->UploadCameraResources(cl);

    // Deferred data
    if (m_deferred)
    {
        cl->UpdateBuffer(m_deferred->GetDeferredShadingConstantsBuffer(), &m_deferred->GetDeferredShadingConstants(),
                         sizeof(DeferredShadingRDGPass::DeferredShadingConstants));
    }

    // Post process data
    if (m_post_process)
    {
        cl->UpdateBuffer(m_post_process->GetConstantsBuffer(), &m_post_process->GetExposureConstants(),
                         sizeof(PostProcessRDGPass::ExposureConstant));
    }

    // GTAO data
    if (m_gtao)
    {
        cl->UpdateBuffer(m_gtao->GetConstantsBuffer(), &m_gtao->GetGTAOConstants(), sizeof(GTAORDGPass::GTAOConstant));
    }

    // TAA data
    if (m_geometry)
    {
        cl->UpdateBuffer(m_geometry->GetTAAPrevCurrOffsetBuffer(), &m_taa_prev_curr_offset,
                         sizeof(TAARDGPass::TAAPrevCurrOffset));
    }
    if (m_taa)
    {
        TAARDGPass::TAAConstants taa_constants{};
        taa_constants.history_valid = (m_first_frame || m_initialize_history) ? 0.0f : 1.0f;
        taa_constants.static_curr_weight = 0.08f;
        taa_constants.velocity_scale = 120.0f;
        taa_constants.velocity_disocclusion_threshold = 0.03f;
        cl->UpdateBuffer(m_taa->GetTAAConstantsBuffer(), &taa_constants, sizeof(taa_constants));
    }

    if (m_first_frame)
    {
        // Deferred IBL data
        if (m_deferred)
        {
            cl->UpdateBuffer(m_deferred->GetDiffuseIrradianceSH3Buffer(),
                             &m_deferred->GetDiffuseIrradianceSH3Constants(),
                             sizeof(DeferredShadingRDGPass::DiffuseIrradianceSH3));

            // Prefiltered irradiance env map
            if (m_deferred)
            {
                TextureUpdateDesc desc2{};
                desc2.first_layer = 0;
                desc2.layer_count = 6;
                desc2.first_mip_level = 0;
                desc2.mip_level_count = m_deferred->GetPrefilteredIrradianceEnvMap()->mip_map_level;
                desc2.size = sizeof(char) * m_deferred->GetPrefilteredIrradianceEnvMapData().raw_data.size();
                desc2.texture_data_desc = &m_deferred->GetPrefilteredIrradianceEnvMapData();
                cl->UpdateTexture(m_deferred->GetPrefilteredIrradianceEnvMap(), desc2);
            }

            // BRDF LUT
            if (m_deferred)
            {
                TextureUpdateDesc desc2{};
                desc2.first_layer = 0;
                desc2.layer_count = 1;
                desc2.first_mip_level = 0;
                desc2.mip_level_count = 1;
                desc2.size = sizeof(char) * m_deferred->GetBRDFLUTDataDesc().raw_data.size();
                desc2.texture_data_desc = &m_deferred->GetBRDFLUTDataDesc();
                cl->UpdateTexture(m_deferred->GetBRDFLUT(), desc2);
            }
        }
    }
}
