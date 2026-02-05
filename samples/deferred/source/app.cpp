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

    deferred = std::make_unique<DeferredShadingPass>(rhi);
    ssao = std::make_unique<AmbientOcclusionPass>(rhi);
    post_process = std::make_unique<PostProcessingPass>(rhi);
    antialiasing = std::make_unique<AntialiasingPass>(rhi);
    scene = std::make_unique<SceneData>(renderer->GetSceneManager());
}

void Render::UpdatePipelineResources()
{

    auto cam = scene->scene_camera;

    // taa jitter

    auto &jitter_offset = antialiasing->GetJitterOffset();
    auto view = cam->GetViewMatrix();
    auto proj = cam->GetProjectionMatrix();
    f32 offset_x = (jitter_offset.x - 0.5f) / width;
    f32 offset_y = (jitter_offset.y - 0.5f) / height;

    antialiasing->taa_prev_curr_offset.prev_offset = antialiasing->taa_prev_curr_offset.curr_offset;
    antialiasing->taa_prev_curr_offset.curr_offset = Math::float2{offset_x, offset_y};
    proj._13 += offset_x;
    proj._23 += offset_y;

    auto vp = view * proj;
    auto inverse_vp = vp.Invert();

    scene->m_scene_manager->camera_ub.prev_vp = scene->m_scene_manager->camera_ub.vp;
    scene->m_scene_manager->camera_ub.vp = vp;

    scene->m_scene_manager->camera_ub.camera_pos = cam->GetPosition();
    scene->m_scene_manager->camera_ub.ev100 = cam->GetEv100();

    post_process->exposure_constants.exposure_ev100__ = Math::float4(cam->GetExposure(), cam->GetEv100(), 0.0, 0.0);

    deferred->deferred_shading_constants.camera_pos = Math::float4(cam->GetPosition());
    deferred->deferred_shading_constants.inverse_vp = inverse_vp;

    ssao->ssao_constansts.proj = proj;
    ssao->ssao_constansts.inv_proj = proj.Invert();
    ssao->ssao_constansts.view = view;
    ssao->ssao_constansts.noise_scale_x = (f32)width / AmbientOcclusionPass::SSAO_NOISE_TEX_WIDTH;
    ssao->ssao_constansts.noise_scale_y = (f32)height / AmbientOcclusionPass::SSAO_NOISE_TEX_HEIGHT;

    post_process->auto_exposure_pass->luminance_histogram_constants.width = width;
    post_process->auto_exposure_pass->luminance_histogram_constants.height = height;
    post_process->auto_exposure_pass->luminance_histogram_constants.pixelCount = width * height;

    post_process->auto_exposure_pass->luminance_histogram_constants.maxLuminance = 20000.0f;

    post_process->auto_exposure_pass->luminance_histogram_constants.timeCoeff = 0.5f;
}

void Render::run()
{

    bool first_frame = true;
    ResourceUploadPass resource_upload_pass;

    while (!window->ShouldClose())
    {
        scene->scene_camera_controller->ProcessInput(window.get());

        rhi->AcquireNextFrame(swap_chain);
        UpdatePipelineResources();
        // Reset FrameGraph for new frame
        frame_graph->Reset();

        // Import resources into FrameGraph using pass methods
        Horizon::Backend::TextureHandle gbuffer0_handle, gbuffer1_handle, gbuffer2_handle, gbuffer3_handle,
            gbuffer4_handle, depth_handle, shading_color_handle, brdf_lut_handle, prefiltered_env_handle;
        Horizon::Backend::RenderTargetHandle gbuffer0_rt_handle, gbuffer1_rt_handle, gbuffer2_rt_handle,
            gbuffer3_rt_handle, gbuffer4_rt_handle, depth_rt_handle;

        deferred->ImportResources(frame_graph.get(), gbuffer0_handle, gbuffer1_handle, gbuffer2_handle, gbuffer3_handle,
                                  gbuffer4_handle, depth_handle, shading_color_handle, gbuffer0_rt_handle,
                                  gbuffer1_rt_handle, gbuffer2_rt_handle, gbuffer3_rt_handle, gbuffer4_rt_handle,
                                  depth_rt_handle, brdf_lut_handle, prefiltered_env_handle);

        Horizon::Backend::TextureHandle ssao_factor_handle, ssao_blur_handle, ssao_noise_handle;
        ssao->ImportResources(frame_graph.get(), ssao_factor_handle, ssao_blur_handle, ssao_noise_handle);

        Horizon::Backend::TextureHandle pp_color_handle;
        post_process->ImportResources(frame_graph.get(), pp_color_handle);

        Horizon::Backend::BufferHandle histogram_buffer_handle, adapted_luminance_handle;
        post_process->auto_exposure_pass->ImportResources(frame_graph.get(), histogram_buffer_handle,
                                                           adapted_luminance_handle);

        Horizon::Backend::TextureHandle output_color_handle, previous_color_handle;
        antialiasing->ImportResources(frame_graph.get(), output_color_handle, previous_color_handle);

        auto swapchain_handle =
            frame_graph->ImportTexture("swapchain" + std::to_string(swap_chain->current_frame_index), swap_chain->GetRenderTarget()->GetTexture());

        // Resource Upload Pass
        frame_graph->AddPass(
            "Resource Upload",
            [&resource_upload_pass, shading_color_handle, pp_color_handle, ssao_factor_handle, ssao_blur_handle,
             output_color_handle, previous_color_handle, ssao_noise_handle, brdf_lut_handle, prefiltered_env_handle,
             histogram_buffer_handle, adapted_luminance_handle, first_frame](
                Horizon::Backend::FrameGraphBuilder &builder) {
                resource_upload_pass.Setup(builder, shading_color_handle, pp_color_handle, ssao_factor_handle,
                                           ssao_blur_handle, output_color_handle, previous_color_handle,
                                           ssao_noise_handle, brdf_lut_handle, prefiltered_env_handle,
                                           histogram_buffer_handle, adapted_luminance_handle, first_frame);
            },
            [&resource_upload_pass, this, first_frame](CommandList *cl,
                                                       Horizon::Backend::FrameGraphBuilder &builder) {
                resource_upload_pass.Execute(cl, scene->m_scene_manager, deferred.get(), ssao.get(),
                                             post_process.get(), antialiasing.get(), first_frame);
            });

        // Geometry Pass
        frame_graph->AddPass(
            "Geometry Pass",
            [this, gbuffer0_rt_handle, gbuffer1_rt_handle, gbuffer2_rt_handle, gbuffer3_rt_handle,
             gbuffer4_rt_handle, depth_rt_handle, gbuffer0_handle, gbuffer1_handle, gbuffer2_handle, gbuffer3_handle,
             gbuffer4_handle, depth_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                deferred->SetupGeometryPass(builder, gbuffer0_rt_handle, gbuffer1_rt_handle, gbuffer2_rt_handle,
                                            gbuffer3_rt_handle, gbuffer4_rt_handle, depth_rt_handle, gbuffer0_handle,
                                            gbuffer1_handle, gbuffer2_handle, gbuffer3_handle, gbuffer4_handle,
                                            depth_handle);
            },
            [this, gbuffer0_rt_handle, gbuffer1_rt_handle, gbuffer2_rt_handle, gbuffer3_rt_handle,
             gbuffer4_rt_handle, depth_rt_handle](CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder) {
                deferred->ExecuteGeometryPass(cl, builder, gbuffer0_rt_handle, gbuffer1_rt_handle, gbuffer2_rt_handle,
                                               gbuffer3_rt_handle, gbuffer4_rt_handle, depth_rt_handle,
                                               scene->m_scene_manager, sampler, antialiasing->taa_prev_curr_offset_buffer);
            });

        // SSAO Pass
        frame_graph->AddPass(
            "SSAO Pass",
            [this, depth_handle, gbuffer0_handle, ssao_factor_handle,
             ssao_noise_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                ssao->SetupSSAOPass(builder, depth_handle, gbuffer0_handle, ssao_factor_handle, ssao_noise_handle);
            },
            [this, depth_handle, gbuffer0_handle, ssao_factor_handle,
             ssao_noise_handle](CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder) {
                ssao->ExecuteSSAOPass(cl, builder, depth_handle, gbuffer0_handle, ssao_factor_handle, ssao_noise_handle,
                                     sampler);
            });

        // SSAO Blur Pass
        frame_graph->AddPass(
            "SSAO Blur Pass",
            [this, ssao_factor_handle, ssao_blur_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                ssao->SetupSSAOBlurPass(builder, ssao_factor_handle, ssao_blur_handle);
            },
            [this, ssao_factor_handle, ssao_blur_handle](CommandList *cl,
                                                        Horizon::Backend::FrameGraphBuilder &builder) {
                ssao->ExecuteSSAOBlurPass(cl, builder, ssao_factor_handle, ssao_blur_handle);
            });

        // Deferred Shading Pass
        frame_graph->AddPass(
            "Deferred Shading Pass",
            [this, gbuffer0_handle, gbuffer1_handle, gbuffer2_handle, gbuffer3_handle, depth_handle,
             shading_color_handle, ssao_blur_handle, brdf_lut_handle,
             prefiltered_env_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                deferred->SetupDeferredShadingPass(builder, gbuffer0_handle, gbuffer1_handle, gbuffer2_handle,
                                                    gbuffer3_handle, depth_handle, shading_color_handle,
                                                    ssao_blur_handle, brdf_lut_handle, prefiltered_env_handle);
            },
            [this, gbuffer0_handle, gbuffer1_handle, gbuffer2_handle, gbuffer3_handle, depth_handle,
             shading_color_handle, ssao_blur_handle, brdf_lut_handle,
             prefiltered_env_handle](CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder) {
                deferred->ExecuteDeferredShadingPass(cl, builder, gbuffer0_handle, gbuffer1_handle, gbuffer2_handle,
                                                       gbuffer3_handle, depth_handle, shading_color_handle,
                                                       ssao_blur_handle, brdf_lut_handle, prefiltered_env_handle,
                                                       scene->m_scene_manager);
            });

        // Luminance Histogram Pass
        frame_graph->AddPass(
            "Luminance Histogram Pass",
            [this, shading_color_handle, histogram_buffer_handle,
             adapted_luminance_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                post_process->auto_exposure_pass->SetupLuminanceHistogramPass(
                    builder, shading_color_handle, histogram_buffer_handle, adapted_luminance_handle);
            },
            [this, shading_color_handle, histogram_buffer_handle,
             adapted_luminance_handle](CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder) {
                post_process->auto_exposure_pass->ExecuteLuminanceHistogramPass(
                    cl, builder, shading_color_handle, histogram_buffer_handle, adapted_luminance_handle);
            });

        // Luminance Average Pass
        frame_graph->AddPass(
            "Luminance Average Pass",
            [this, histogram_buffer_handle,
             adapted_luminance_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                post_process->auto_exposure_pass->SetupLuminanceAveragePass(builder, histogram_buffer_handle,
                                                                            adapted_luminance_handle);
            },
            [this, histogram_buffer_handle,
             adapted_luminance_handle](CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder) {
                post_process->auto_exposure_pass->ExecuteLuminanceAveragePass(cl, builder, histogram_buffer_handle,
                                                                              adapted_luminance_handle);
            });

        // Post Process Pass
        frame_graph->AddPass(
            "Post Process Pass",
            [this, shading_color_handle, pp_color_handle,
             adapted_luminance_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                post_process->SetupPostProcessPass(builder, shading_color_handle, pp_color_handle,
                                                   adapted_luminance_handle);
            },
            [this, shading_color_handle, pp_color_handle,
             adapted_luminance_handle](CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder) {
                post_process->ExecutePostProcessPass(cl, builder, shading_color_handle, pp_color_handle,
                                                     adapted_luminance_handle);
            });

        // TAA Pass
        frame_graph->AddPass(
            "TAA Pass",
            [this, previous_color_handle, pp_color_handle, gbuffer4_handle,
             output_color_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                antialiasing->SetupTAAPass(builder, previous_color_handle, pp_color_handle, gbuffer4_handle,
                                           output_color_handle);
            },
            [this, previous_color_handle, pp_color_handle, gbuffer4_handle,
             output_color_handle](CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder) {
                antialiasing->ExecuteTAAPass(cl, builder, previous_color_handle, pp_color_handle, gbuffer4_handle,
                                             output_color_handle);
            });

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
            [this, output_color_handle, swapchain_handle,
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
                cl->InsertBarrier(barrier);
            });

        // Compile and execute FrameGraph
        // FrameGraph::Execute() automatically:
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