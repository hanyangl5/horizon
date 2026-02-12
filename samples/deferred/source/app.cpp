#include "app.h"
#include "renderpasses/resource_upload_pass.h"

#include <core/path.h>

Horizon::Path asset_path = ASSET_DIR;
Horizon::Path shader_dir = SHADER_DIR;
u32 width = 1600, height = 900;

void Render::InitAPI()
{
    rhi = renderer->GetRhi();
    frame_graph = std::make_unique<Horizon::Backend::FrameGraph>(rhi);
}

void Render::InitResources()
{
    InitPipelineResources();
}

void Render::InitPipelineResources()
{

    swap_chain = rhi->CreateSwapChain(SwapChainCreateInfo{2});

    {

        SamplerDesc sampler_desc{};
        sampler_desc.min_filter = FilterType::FILTER_LINEAR;
        sampler_desc.mag_filter = FilterType::FILTER_LINEAR;
        sampler_desc.mip_map_mode = MipMapMode::MIPMAP_MODE_LINEAR;
        sampler_desc.address_u = AddressMode::ADDRESS_MODE_REPEAT;
        sampler_desc.address_v = AddressMode::ADDRESS_MODE_REPEAT;
        sampler_desc.address_w = AddressMode::ADDRESS_MODE_REPEAT;

        sampler = rhi->CreateSampler(sampler_desc);
    }

    scene = std::make_unique<SceneData>(renderer->GetSceneManager());

    // Create RDG Passes
    geometry_pass = std::make_unique<DeferredShadingGeometryPass>(rhi, scene->m_scene_manager, sampler);
    deferred_shading_pass = std::make_unique<DeferredShadingRDGPass>(rhi, scene->m_scene_manager);
    ssao_pass = std::make_unique<SSAORDGPass>(rhi, sampler);
    ssao_blur_pass = std::make_unique<SSAOBlurRDGPass>(rhi);
    post_process_pass = std::make_unique<PostProcessRDGPass>(rhi);
    luminance_histogram_pass = std::make_unique<LuminanceHistogramRDGPass>(rhi);
    luminance_average_pass = std::make_unique<LuminanceAverageRDGPass>(rhi);
    taa_pass = std::make_unique<TAARDGPass>(rhi);
    resource_upload_pass = std::make_unique<ResourceUploadRDGPass>(rhi, scene->m_scene_manager);
}

void Render::UpdatePipelineResources()
{
    auto cam = scene->scene_camera;

    // TAA jitter
    auto &jitter_offset = taa_pass->GetJitterOffset();
    auto view = cam->GetViewMatrix();
    auto proj = cam->GetProjectionMatrix();
    f32 offset_x = (jitter_offset.x - 0.5f) / width;
    f32 offset_y = (jitter_offset.y - 0.5f) / height;

    TAARDGPass::TAAPrevCurrOffset taa_offset{};
    taa_offset.prev_offset = taa_offset.curr_offset; // This will be updated properly in resource upload
    taa_offset.curr_offset = Math::float2{offset_x, offset_y};
    proj._13 += offset_x;
    proj._23 += offset_y;

    auto vp = view * proj;
    auto inverse_vp = vp.Invert();

    scene->m_scene_manager->camera_ub.prev_vp = scene->m_scene_manager->camera_ub.vp;
    scene->m_scene_manager->camera_ub.vp = vp;
    scene->m_scene_manager->camera_ub.camera_pos = cam->GetPosition();
    scene->m_scene_manager->camera_ub.ev100 = cam->GetEv100();

    // Post process constants
    post_process_pass->GetExposureConstants().exposure_ev100__ =
        Math::float4(cam->GetExposure(), cam->GetEv100(), 0.0, 0.0);

    // Deferred shading constants
    deferred_shading_pass->GetDeferredShadingConstants().camera_pos = Math::float4(cam->GetPosition());
    deferred_shading_pass->GetDeferredShadingConstants().inverse_vp = inverse_vp;

    // SSAO constants
    ssao_pass->GetSSAOConstants().proj = proj;
    ssao_pass->GetSSAOConstants().inv_proj = proj.Invert();
    ssao_pass->GetSSAOConstants().view = view;
    ssao_pass->GetSSAOConstants().noise_scale_x = (f32)width / SSAORDGPass::SSAO_NOISE_TEX_WIDTH;
    ssao_pass->GetSSAOConstants().noise_scale_y = (f32)height / SSAORDGPass::SSAO_NOISE_TEX_HEIGHT;

    // Luminance histogram constants
    luminance_histogram_pass->GetLuminanceHistogramConstants().width = width;
    luminance_histogram_pass->GetLuminanceHistogramConstants().height = height;
    luminance_histogram_pass->GetLuminanceHistogramConstants().pixelCount = width * height;
    luminance_histogram_pass->GetLuminanceHistogramConstants().maxLuminance = 20000.0f;
    luminance_histogram_pass->GetLuminanceHistogramConstants().timeCoeff = 0.5f;
}

void Render::run()
{
    bool first_frame = true;

    while (!window->ShouldClose())
    {
        scene->scene_camera_controller->ProcessInput(window.get());

        rhi->AcquireNextFrame(swap_chain);
        UpdatePipelineResources();
        // Reset FrameGraph for new frame
        frame_graph->Reset();

        // Import resources into FrameGraph using pass methods
        geometry_pass->ImportResources(frame_graph.get());
        deferred_shading_pass->ImportResources(frame_graph.get());
        ssao_pass->ImportResources(frame_graph.get());
        ssao_blur_pass->ImportResources(frame_graph.get());
        post_process_pass->ImportResources(frame_graph.get());
        luminance_histogram_pass->ImportResources(frame_graph.get());
        luminance_average_pass->ImportResources(frame_graph.get());
        taa_pass->ImportResources(frame_graph.get());
        resource_upload_pass->ImportResources(frame_graph.get());

        // Get resource handles from passes
        auto gbuffer0_handle = geometry_pass->GetGBuffer0Handle();
        auto gbuffer1_handle = geometry_pass->GetGBuffer1Handle();
        auto gbuffer2_handle = geometry_pass->GetGBuffer2Handle();
        auto gbuffer3_handle = geometry_pass->GetGBuffer3Handle();
        auto gbuffer4_handle = geometry_pass->GetGBuffer4Handle();
        auto depth_handle = geometry_pass->GetDepthHandle();
        auto shading_color_handle = deferred_shading_pass->GetShadingColorHandle();
        auto ssao_factor_handle = ssao_pass->GetSSAOFactorHandle();
        auto ssao_blur_handle = ssao_blur_pass->GetOutputHandle();
        auto ssao_noise_handle = ssao_pass->GetSSAONoiseHandle();
        auto brdf_lut_handle = deferred_shading_pass->GetBRDFLUTHandle();
        auto prefiltered_env_handle = deferred_shading_pass->GetPrefilteredEnvHandle();
        auto pp_color_handle = post_process_pass->GetPPColorHandle();
        auto histogram_buffer_handle = luminance_histogram_pass->GetHistogramBufferHandle();
        auto adapted_luminance_handle = luminance_histogram_pass->GetAdaptedLuminanceHandle();
        auto output_color_handle = taa_pass->GetOutputColorHandle();
        auto previous_color_handle = taa_pass->GetPreviousColorHandle();

        // Set input handles for passes
        deferred_shading_pass->SetGBufferHandles(gbuffer0_handle, gbuffer1_handle, gbuffer2_handle, gbuffer3_handle,
                                                 depth_handle);
        deferred_shading_pass->SetSSAOBlurHandle(ssao_blur_handle);
        ssao_pass->SetInputHandles(depth_handle, gbuffer0_handle);
        ssao_blur_pass->SetInputHandle(ssao_factor_handle);
        post_process_pass->SetInputHandles(shading_color_handle, adapted_luminance_handle);
        luminance_histogram_pass->SetInputHandle(shading_color_handle);
        luminance_average_pass->SetInputHandles(histogram_buffer_handle, adapted_luminance_handle);
        luminance_average_pass->SetLuminanceHistogramPass(luminance_histogram_pass.get());
        taa_pass->SetInputHandles(previous_color_handle, pp_color_handle, gbuffer4_handle);

        // Set resource handles for resource upload pass
        resource_upload_pass->SetResourceHandles(shading_color_handle, pp_color_handle, ssao_factor_handle,
                                                 ssao_blur_handle, output_color_handle, previous_color_handle,
                                                 ssao_noise_handle, brdf_lut_handle, prefiltered_env_handle,
                                                 histogram_buffer_handle, adapted_luminance_handle);
        resource_upload_pass->SetPassPointers(deferred_shading_pass.get(), ssao_pass.get(), post_process_pass.get(),
                                              luminance_histogram_pass.get(), taa_pass.get());
        resource_upload_pass->SetFirstFrame(first_frame);

        // Update TAA offset (this should be done in UpdatePipelineResources, but we set it here for resource upload)
        static TAARDGPass::TAAPrevCurrOffset taa_prev_offset{};
        TAARDGPass::TAAPrevCurrOffset taa_offset{};
        taa_offset.prev_offset = taa_prev_offset.curr_offset;
        auto cam = scene->scene_camera;
        auto &jitter_offset = taa_pass->GetJitterOffset();
        f32 offset_x = (jitter_offset.x - 0.5f) / width;
        f32 offset_y = (jitter_offset.y - 0.5f) / height;
        taa_offset.curr_offset = Math::float2{offset_x, offset_y};
        taa_prev_offset = taa_offset;
        resource_upload_pass->SetTAAPrevCurrOffset(taa_offset);

        auto swapchain_handle = frame_graph->ImportTexture(
            "swapchain" + std::to_string(swap_chain->current_frame_index), swap_chain->GetRenderTarget()->GetTexture());

        // Add passes to FrameGraph using RDGPass
        frame_graph->AddPass(resource_upload_pass.get());
        frame_graph->AddPass(geometry_pass.get());
        frame_graph->AddPass(ssao_pass.get());
        frame_graph->AddPass(ssao_blur_pass.get());
        frame_graph->AddPass(deferred_shading_pass.get());
        frame_graph->AddPass(luminance_histogram_pass.get());
        frame_graph->AddPass(luminance_average_pass.get());
        frame_graph->AddPass(post_process_pass.get());
        frame_graph->AddPass(taa_pass.get());

        // Copy to Swapchain Pass
        frame_graph->AddPass(
            "Copy to Swapchain",
            // Setup: Declare resource states
            [output_color_handle, swapchain_handle,
             previous_color_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                // Source needs to be COPY_SOURCE for CopyTexture
                builder.ReadTexture(output_color_handle, ResourceState::RESOURCE_STATE_COPY_SOURCE);
                builder.WriteTexture(swapchain_handle, ResourceState::RESOURCE_STATE_COPY_DEST);
                builder.WriteTexture(previous_color_handle, ResourceState::RESOURCE_STATE_COPY_DEST);
            },
            // Execute: Copy textures
            [output_color_handle, swapchain_handle,
             previous_color_handle](CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder) {
                // Copy to swapchain and previous frame
                cl->CopyTexture(builder.GetTexture(output_color_handle), builder.GetTexture(swapchain_handle));
                cl->CopyTexture(builder.GetTexture(output_color_handle), builder.GetTexture(previous_color_handle));

                // Transition swapchain image from COPY_DEST to PRESENT for vkQueuePresentKHR
                Horizon::BarrierDesc barrier{};
                Horizon::TextureBarrierDesc swapchain_barrier{};
                swapchain_barrier.texture = builder.GetTexture(swapchain_handle);
                swapchain_barrier.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                swapchain_barrier.dst_state = ResourceState::RESOURCE_STATE_PRESENT;
                swapchain_barrier.first_mip_level = 0;
                swapchain_barrier.mip_level_count = 1;
                swapchain_barrier.first_layer = 0;
                swapchain_barrier.layer_count = 1;
                swapchain_barrier.queue = CommandQueueType::GRAPHICS;
                swapchain_barrier.queue_op = Horizon::QueueOp::IGNORED;
                barrier.texture_memory_barriers.push_back(swapchain_barrier);
                // borrow swapchain barrier desc
                swapchain_barrier.texture = builder.GetTexture(previous_color_handle);
                swapchain_barrier.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                swapchain_barrier.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                barrier.texture_memory_barriers.push_back(swapchain_barrier);
                cl->InsertBarrier(barrier);
            });

        // Compile and execute FrameGraph
        // FrameGraph::Execute() automatically:f
        // 1. Inserts barriers between passes
        // 2. Executes all passes
        // 3. Submits command lists grouped by queue type
        frame_graph->Compile();
        frame_graph->Execute();

        // Present
        {
            QueuePresentInfo opaque_pass_ci{};
            opaque_pass_ci.swap_chain = swap_chain;
            rhi->Present(opaque_pass_ci);
        }

        rhi->WaitGpuExecution(CommandQueueType::GRAPHICS);
        if (first_frame)
        {
            first_frame = false;
        }
        // Horizon::RDC::EndFrameCapture();
    }

    LOG_INFO("draw done");
}

int main()
{
    Render horizon_pipeline;
    horizon_pipeline.Init();
    horizon_pipeline.run();
}