#include "resource_upload_pass.h"
#include "deferredshading.h"
#include "ambient_occlusion.h"
#include "post_process.h"
#include "antialiasing.h"
#include <scene/scene_manager/scene_manager.h>

void ResourceUploadPass::Setup(Horizon::Backend::FrameGraphBuilder &builder,
                               Horizon::Backend::TextureHandle shading_color_handle,
                               Horizon::Backend::TextureHandle pp_color_handle,
                               Horizon::Backend::TextureHandle ssao_factor_handle,
                               Horizon::Backend::TextureHandle ssao_blur_handle,
                               Horizon::Backend::TextureHandle output_color_handle,
                               Horizon::Backend::TextureHandle previous_color_handle,
                               Horizon::Backend::TextureHandle ssao_noise_handle,
                               Horizon::Backend::TextureHandle brdf_lut_handle,
                               Horizon::Backend::TextureHandle prefiltered_env_handle,
                               Horizon::Backend::BufferHandle histogram_buffer_handle,
                               Horizon::Backend::BufferHandle adapted_luminance_handle, bool first_frame)
{
    builder.WriteTexture(shading_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteTexture(pp_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteTexture(ssao_factor_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteTexture(ssao_blur_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteTexture(output_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);

    if (first_frame)
    {
        builder.WriteTexture(previous_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
        builder.WriteTexture(ssao_noise_handle, ResourceState::RESOURCE_STATE_COPY_DEST);
        builder.WriteTexture(brdf_lut_handle, ResourceState::RESOURCE_STATE_COPY_DEST);
        builder.WriteTexture(prefiltered_env_handle, ResourceState::RESOURCE_STATE_COPY_DEST);
    }
    else
    {
        builder.WriteTexture(previous_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    }

    builder.WriteBuffer(histogram_buffer_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteBuffer(adapted_luminance_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
}

void ResourceUploadPass::Execute(CommandList *cl, Horizon::SceneManager *scene_manager, DeferredShadingPass *deferred,
                                 AmbientOcclusionPass *ssao, PostProcessingPass *post_process,
                                 AntialiasingPass *antialiasing, bool first_frame)
{
    // upload textures, vertex/index buffer
    if (first_frame)
    {
        scene_manager->UploadBuiltInResources(cl);
        scene_manager->UploadMeshResources(cl);
    }
    // scene data
    scene_manager->UploadLightResources(cl);
    scene_manager->UploadCameraResources(cl);

    // deferred data
    cl->UpdateBuffer(deferred->deferred_shading_constants_buffer, &deferred->deferred_shading_constants,
                     sizeof(deferred->deferred_shading_constants));
    // post process data
    cl->UpdateBuffer(post_process->exposure_constants_buffer, &post_process->exposure_constants,
                     sizeof(PostProcessingPass::ExposureConstant));
    cl->UpdateBuffer(post_process->auto_exposure_pass->luminance_histogram_constants_buffer,
                     &post_process->auto_exposure_pass->luminance_histogram_constants,
                     sizeof(AutoExposure::LuminanceHistogramConstants));
    cl->UpdateBuffer(ssao->ssao_constants_buffer, &ssao->ssao_constansts, sizeof(AmbientOcclusionPass::SSAOConstant));
    cl->UpdateBuffer(antialiasing->taa_prev_curr_offset_buffer, &antialiasing->taa_prev_curr_offset,
                     sizeof(AntialiasingPass::TAAPrevCurrOffset));

    cl->ClearBuffer(post_process->auto_exposure_pass->histogram_buffer, 0.0f);
    cl->ClearBuffer(post_process->auto_exposure_pass->adapted_muminance_buffer, 0.0f);
    if (first_frame)
    {
        cl->UpdateBuffer(deferred->diffuse_irradiance_sh3_buffer, &deferred->diffuse_irradiance_sh3_constants,
                         sizeof(deferred->diffuse_irradiance_sh3_constants));
        {
            TextureUpdateDesc desc{};
            desc.texture_data_desc = &ssao->ssao_noise_tex_data_desc;
            desc.size = GetBytesFromTextureFormat(ssao->ssao_noise_tex->m_format) *
                        AmbientOcclusionPass::SSAO_NOISE_TEX_WIDTH * AmbientOcclusionPass::SSAO_NOISE_TEX_HEIGHT;
            cl->UpdateTexture(ssao->ssao_noise_tex, desc);
        }
        // prefilered_irradiance_env_ma
        {
            TextureUpdateDesc desc2{};
            desc2.first_layer = 0;
            desc2.layer_count = 6;
            desc2.first_mip_level = 0;
            desc2.mip_level_count = deferred->prefiltered_irradiance_env_map->mip_map_level;
            desc2.size = sizeof(char) * deferred->prefilered_irradiance_env_map_data.raw_data.size();
            desc2.texture_data_desc = &deferred->prefilered_irradiance_env_map_data;
            cl->UpdateTexture(deferred->prefiltered_irradiance_env_map, desc2);
        }
        {
            TextureUpdateDesc desc2{};
            desc2.first_layer = 0;
            desc2.layer_count = 1;
            desc2.first_mip_level = 0;
            desc2.mip_level_count = 1;
            desc2.size = sizeof(char) * deferred->brdf_lut_data_desc.raw_data.size();
            desc2.texture_data_desc = &deferred->brdf_lut_data_desc;
            cl->UpdateTexture(deferred->brdf_lut, desc2);
        }
    }
}
