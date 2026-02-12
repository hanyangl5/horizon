#include "resource_upload_pass.h"
#include "deferred_shading_rdg_pass.h"
#include "luminance_histogram_rdg_pass.h"
#include "post_process_rdg_pass.h"
#include "ssao_rdg_pass.h"
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
    Horizon::Backend::TextureHandle ssao_factor, Horizon::Backend::TextureHandle ssao_blur,
    Horizon::Backend::TextureHandle output_color, Horizon::Backend::TextureHandle previous_color,
    Horizon::Backend::TextureHandle ssao_noise, Horizon::Backend::TextureHandle brdf_lut,
    Horizon::Backend::TextureHandle prefiltered_env, Horizon::Backend::BufferHandle histogram_buffer,
    Horizon::Backend::BufferHandle adapted_luminance)
{
    m_shading_color_handle = shading_color;
    m_pp_color_handle = pp_color;
    m_ssao_factor_handle = ssao_factor;
    m_ssao_blur_handle = ssao_blur;
    m_output_color_handle = output_color;
    m_previous_color_handle = previous_color;
    m_ssao_noise_handle = ssao_noise;
    m_brdf_lut_handle = brdf_lut;
    m_prefiltered_env_handle = prefiltered_env;
    m_histogram_buffer_handle = histogram_buffer;
    m_adapted_luminance_handle = adapted_luminance;
}

void ResourceUploadRDGPass::SetPassPointers(DeferredShadingRDGPass *deferred, SSAORDGPass *ssao,
                                            PostProcessRDGPass *post_process,
                                            LuminanceHistogramRDGPass *luminance_histogram, TAARDGPass *taa)
{
    m_deferred = deferred;
    m_ssao = ssao;
    m_post_process = post_process;
    m_luminance_histogram = luminance_histogram;
    m_taa = taa;
}

void ResourceUploadRDGPass::Setup(Horizon::Backend::FrameGraphBuilder &builder)
{
    builder.WriteTexture(m_shading_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteTexture(m_pp_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteTexture(m_ssao_factor_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteTexture(m_ssao_blur_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteTexture(m_output_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);

    if (m_first_frame)
    {
        builder.WriteTexture(m_previous_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
        builder.WriteTexture(m_ssao_noise_handle, ResourceState::RESOURCE_STATE_COPY_DEST);
        builder.WriteTexture(m_brdf_lut_handle, ResourceState::RESOURCE_STATE_COPY_DEST);
        builder.WriteTexture(m_prefiltered_env_handle, ResourceState::RESOURCE_STATE_COPY_DEST);
    }
    else
    {
        // builder.WriteTexture(m_previous_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    }

    builder.WriteBuffer(m_histogram_buffer_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteBuffer(m_adapted_luminance_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
}

void ResourceUploadRDGPass::Execute(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder)
{
    // Upload textures, vertex/index buffer
    if (m_first_frame)
    {
        m_scene_manager->UploadBuiltInResources(cl);
        m_scene_manager->UploadMeshResources(cl);
    }
    // Scene data
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

    // Luminance histogram data
    if (m_luminance_histogram)
    {
        cl->UpdateBuffer(m_luminance_histogram->GetConstantsBuffer(),
                         &m_luminance_histogram->GetLuminanceHistogramConstants(),
                         sizeof(LuminanceHistogramRDGPass::LuminanceHistogramConstants));
    }

    // SSAO data
    if (m_ssao)
    {
        cl->UpdateBuffer(m_ssao->GetConstantsBuffer(), &m_ssao->GetSSAOConstants(), sizeof(SSAORDGPass::SSAOConstant));
    }

    // TAA data
    if (m_taa)
    {
        cl->UpdateBuffer(m_taa->GetTAAPrevCurrOffsetBuffer(), &m_taa_prev_curr_offset,
                         sizeof(TAARDGPass::TAAPrevCurrOffset));
    }

    // Clear buffers
    if (m_luminance_histogram)
    {
        cl->ClearBuffer(m_luminance_histogram->GetHistogramBuffer(), 0.0f);
        cl->ClearBuffer(m_luminance_histogram->GetAdaptedLuminanceBuffer(), 0.0f);
    }

    if (m_first_frame)
    {
        // Deferred IBL data
        if (m_deferred)
        {
            cl->UpdateBuffer(m_deferred->GetDiffuseIrradianceSH3Buffer(),
                             &m_deferred->GetDiffuseIrradianceSH3Constants(),
                             sizeof(DeferredShadingRDGPass::DiffuseIrradianceSH3));

            // SSAO noise texture
            if (m_ssao)
            {
                TextureUpdateDesc desc{};
                desc.texture_data_desc = &m_ssao->GetSSAONoiseTexDataDesc();
                desc.size = GetBytesFromTextureFormat(m_ssao->GetSSAONoiseTex()->m_format) * 4 * 4;
                cl->UpdateTexture(m_ssao->GetSSAONoiseTex(), desc);
            }

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
