#include "render_app.h"

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
    ssao->ssao_constansts.noise_scale_x = width / AmbientOcclusionPass::SSAO_NOISE_TEX_WIDTH;
    ssao->ssao_constansts.noise_scale_y = height / AmbientOcclusionPass::SSAO_NOISE_TEX_HEIGHT;

    post_process->auto_exposure_pass->luminance_histogram_constants.width = width;
    post_process->auto_exposure_pass->luminance_histogram_constants.height = height;
    post_process->auto_exposure_pass->luminance_histogram_constants.pixelCount = width * height;

    post_process->auto_exposure_pass->luminance_histogram_constants.maxLuminance = 20000.0f;

    post_process->auto_exposure_pass->luminance_histogram_constants.timeCoeff = 0.5f;
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

        // Import resources into FrameGraph
        auto gbuffer0_rt_handle = frame_graph->ImportRenderTarget("gbuffer0_rt", deferred->gbuffer0);
        auto gbuffer1_rt_handle = frame_graph->ImportRenderTarget("gbuffer1_rt", deferred->gbuffer1);
        auto gbuffer2_rt_handle = frame_graph->ImportRenderTarget("gbuffer2_rt", deferred->gbuffer2);
        auto gbuffer3_rt_handle = frame_graph->ImportRenderTarget("gbuffer3_rt", deferred->gbuffer3);
        auto gbuffer4_rt_handle = frame_graph->ImportRenderTarget("gbuffer4_rt", deferred->gbuffer4);
        auto depth_rt_handle = frame_graph->ImportRenderTarget("depth_rt", deferred->depth);

        // Import textures for state tracking
        auto gbuffer0_handle = frame_graph->ImportTexture("gbuffer0", deferred->gbuffer0->GetTexture());
        auto gbuffer1_handle = frame_graph->ImportTexture("gbuffer1", deferred->gbuffer1->GetTexture());
        auto gbuffer2_handle = frame_graph->ImportTexture("gbuffer2", deferred->gbuffer2->GetTexture());
        auto gbuffer3_handle = frame_graph->ImportTexture("gbuffer3", deferred->gbuffer3->GetTexture());
        auto gbuffer4_handle = frame_graph->ImportTexture("gbuffer4", deferred->gbuffer4->GetTexture());
        auto depth_handle = frame_graph->ImportTexture("depth", deferred->depth->GetTexture());

        auto shading_color_handle = frame_graph->ImportTexture("shading_color", deferred->shading_color_image);
        auto pp_color_handle = frame_graph->ImportTexture("pp_color", post_process->pp_color_image);
        auto ssao_factor_handle = frame_graph->ImportTexture("ssao_factor", ssao->ssao_factor_image);
        auto ssao_blur_handle = frame_graph->ImportTexture("ssao_blur", ssao->ssao_blur_image);
        auto output_color_handle = frame_graph->ImportTexture("output_color", antialiasing->output_color_texture);
        auto previous_color_handle = frame_graph->ImportTexture("previous_color", antialiasing->previous_color_texture);
        auto swapchain_handle = frame_graph->ImportTexture("swapchain", swap_chain->GetRenderTarget()->GetTexture());

        auto ssao_noise_handle = frame_graph->ImportTexture("ssao_noise", ssao->ssao_noise_tex);
        auto brdf_lut_handle = frame_graph->ImportTexture("brdf_lut", deferred->brdf_lut);
        auto prefiltered_env_handle =
            frame_graph->ImportTexture("prefiltered_env", deferred->prefiltered_irradiance_env_map);

        auto histogram_buffer_handle =
            frame_graph->ImportBuffer("histogram_buffer", post_process->auto_exposure_pass->histogram_buffer);
        auto adapted_luminance_handle =
            frame_graph->ImportBuffer("adapted_luminance", post_process->auto_exposure_pass->adapted_muminance_buffer);

        // Resource Upload Pass
        frame_graph->AddPass(
            "Resource Upload",
            // Setup: Declare resource states
            [shading_color_handle, pp_color_handle, ssao_factor_handle, ssao_blur_handle, output_color_handle,
             previous_color_handle, ssao_noise_handle, brdf_lut_handle, prefiltered_env_handle, histogram_buffer_handle,
             adapted_luminance_handle, first_frame](Horizon::Backend::FrameGraphBuilder &builder) {
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
            },
            // Execute: Upload resources
            [this, first_frame](CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder) {
                // upload textures, vertex/index buffer
                if (first_frame)
                {
                    scene->m_scene_manager->UploadBuiltInResources(cl);
                    scene->m_scene_manager->UploadMeshResources(cl);
                }
                // scene data
                scene->m_scene_manager->UploadLightResources(cl);
                scene->m_scene_manager->UploadCameraResources(cl);

                // deferred data
                cl->UpdateBuffer(deferred->deferred_shading_constants_buffer, &deferred->deferred_shading_constants,
                                 sizeof(deferred->deferred_shading_constants));
                // post process data
                cl->UpdateBuffer(post_process->exposure_constants_buffer, &post_process->exposure_constants,
                                 sizeof(PostProcessingPass::ExposureConstant));
                cl->UpdateBuffer(post_process->auto_exposure_pass->luminance_histogram_constants_buffer,
                                 &post_process->auto_exposure_pass->luminance_histogram_constants,
                                 sizeof(AutoExposure::LuminanceHistogramConstants));
                cl->UpdateBuffer(ssao->ssao_constants_buffer, &ssao->ssao_constansts,
                                 sizeof(AmbientOcclusionPass::SSAOConstant));
                cl->UpdateBuffer(antialiasing->taa_prev_curr_offset_buffer, &antialiasing->taa_prev_curr_offset,
                                 sizeof(AntialiasingPass::TAAPrevCurrOffset));

                cl->ClearBuffer(post_process->auto_exposure_pass->histogram_buffer, 0.0f);
                cl->ClearBuffer(post_process->auto_exposure_pass->adapted_muminance_buffer, 0.0f);
                if (first_frame)
                {
                    cl->UpdateBuffer(deferred->diffuse_irradiance_sh3_buffer,
                                     &deferred->diffuse_irradiance_sh3_constants,
                                     sizeof(deferred->diffuse_irradiance_sh3_constants));
                    {
                        TextureUpdateDesc desc{};
                        desc.texture_data_desc = &ssao->ssao_noise_tex_data_desc;
                        desc.size = GetBytesFromTextureFormat(ssao->ssao_noise_tex->m_format) *
                                    AmbientOcclusionPass::SSAO_NOISE_TEX_WIDTH *
                                    AmbientOcclusionPass::SSAO_NOISE_TEX_HEIGHT;
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
            });

        // Setup geometry pass resources
        deferred->geometry_pass->SetResource(scene->m_scene_manager->GetCameraBuffer(), "CameraParamsUb_cb");
        deferred->geometry_pass->SetResource(scene->m_scene_manager->instance_parameter_buffer, "instance_parameter");
        deferred->geometry_pass->SetResource(scene->m_scene_manager->material_description_buffer,
                                             "material_descriptions");
        deferred->geometry_pass->SetResource(sampler, "default_sampler");
        deferred->geometry_pass->SetResource(antialiasing->taa_prev_curr_offset_buffer, "TAAOffsets_cb");

        std::vector<Texture *> material_textures;
        for (auto &tex : scene->m_scene_manager->material_textures)
        {
            material_textures.push_back(tex);
        }
        deferred->geometry_pass->SetBindlessResource(material_textures, "material_textures");

        // Geometry Pass
        frame_graph->AddPass(
            "Geometry Pass",
            // Setup: Declare resource states
            [gbuffer0_rt_handle, gbuffer1_rt_handle, gbuffer2_rt_handle, gbuffer3_rt_handle, gbuffer4_rt_handle,
             depth_rt_handle, gbuffer0_handle, gbuffer1_handle, gbuffer2_handle, gbuffer3_handle, gbuffer4_handle,
             depth_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                builder.UseRenderTarget(gbuffer0_rt_handle);
                builder.UseRenderTarget(gbuffer1_rt_handle);
                builder.UseRenderTarget(gbuffer2_rt_handle);
                builder.UseRenderTarget(gbuffer3_rt_handle);
                builder.UseRenderTarget(gbuffer4_rt_handle);
                builder.UseRenderTarget(depth_rt_handle);

                builder.WriteTexture(gbuffer0_handle, ResourceState::RESOURCE_STATE_RENDER_TARGET);
                builder.WriteTexture(gbuffer1_handle, ResourceState::RESOURCE_STATE_RENDER_TARGET);
                builder.WriteTexture(gbuffer2_handle, ResourceState::RESOURCE_STATE_RENDER_TARGET);
                builder.WriteTexture(gbuffer3_handle, ResourceState::RESOURCE_STATE_RENDER_TARGET);
                builder.WriteTexture(gbuffer4_handle, ResourceState::RESOURCE_STATE_RENDER_TARGET);
                builder.WriteTexture(depth_handle, ResourceState::RESOURCE_STATE_DEPTH_WRITE);
                //
                // builder.ReadTexture(gbuffer0_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
                // builder.ReadTexture(gbuffer1_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
                // builder.ReadTexture(gbuffer2_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
                // builder.ReadTexture(gbuffer3_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
                // builder.ReadTexture(gbuffer4_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
                // builder.ReadTexture(depth_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
            },
            // Execute: Render geometry
            [this, gbuffer0_rt_handle, gbuffer1_rt_handle, gbuffer2_rt_handle, gbuffer3_rt_handle, gbuffer4_rt_handle,
             depth_rt_handle](CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder) {
                RenderPassBeginInfo begin_info{};
                begin_info.render_target_count = 5;
                begin_info.render_area = Rect{0, 0, width, height};
                begin_info.render_targets[0].data = builder.GetRenderTarget(gbuffer0_rt_handle);
                begin_info.render_targets[0].clear_color = {};
                begin_info.render_targets[0].load_op = RenderTargetLoadOp::CLEAR;
                begin_info.render_targets[0].store_op = RenderTargetStoreOp::STORE;
                begin_info.render_targets[1].data = builder.GetRenderTarget(gbuffer1_rt_handle);
                begin_info.render_targets[1].clear_color = {};
                begin_info.render_targets[1].load_op = RenderTargetLoadOp::CLEAR;
                begin_info.render_targets[1].store_op = RenderTargetStoreOp::STORE;
                begin_info.render_targets[2].data = builder.GetRenderTarget(gbuffer2_rt_handle);
                begin_info.render_targets[2].clear_color = {};
                begin_info.render_targets[2].load_op = RenderTargetLoadOp::CLEAR;
                begin_info.render_targets[2].store_op = RenderTargetStoreOp::STORE;
                begin_info.render_targets[3].data = builder.GetRenderTarget(gbuffer3_rt_handle);
                begin_info.render_targets[3].clear_color = {};
                begin_info.render_targets[3].load_op = RenderTargetLoadOp::CLEAR;
                begin_info.render_targets[3].store_op = RenderTargetStoreOp::STORE;
                begin_info.render_targets[4].data = builder.GetRenderTarget(gbuffer4_rt_handle);
                begin_info.render_targets[4].clear_color = {};
                begin_info.render_targets[4].load_op = RenderTargetLoadOp::CLEAR;
                begin_info.render_targets[4].store_op = RenderTargetStoreOp::STORE;
                begin_info.depth_stencil.data = builder.GetRenderTarget(depth_rt_handle);
                begin_info.depth_stencil.clear_color = ClearValueDepthStencil{1.0, 0};
                begin_info.depth_stencil.load_op = RenderTargetLoadOp::CLEAR;
                begin_info.depth_stencil.store_op = RenderTargetStoreOp::STORE;
                begin_info.debug_name = "Geometry Pass";

                cl->BeginRenderPass(begin_info);
                cl->BindPipeline(deferred->geometry_pass);

                for (u32 mesh_data = 0; mesh_data < scene->m_scene_manager->mesh_data.size(); mesh_data++)
                {
                    auto &mesh = scene->m_scene_manager->mesh_data[mesh_data];
                    auto ib = scene->m_scene_manager->index_buffers[mesh.index_buffer_offset];
                    auto vb = scene->m_scene_manager->vertex_buffers[mesh.vertex_buffer_offset];
                    u32 offset = 0;
                    cl->BindVertexBuffers(1, &vb, &offset);
                    cl->BindIndexBuffer(ib, 0);
                    cl->BindPushConstant(deferred->geometry_pass, "mesh_draw_offset", &mesh.draw_offset);

                    cl->DrawIndirectIndexedInstanced(scene->m_scene_manager->indirect_draw_command_buffer1,
                                                     sizeof(DrawIndexedInstancedCommand) * mesh.draw_offset,
                                                     mesh.draw_count, sizeof(DrawIndexedInstancedCommand));
                }

                cl->EndRenderPass();
            });

        // SSAO Pass
        frame_graph->AddPass(
            "SSAO Pass",
            // Setup: Declare resource states
            [depth_handle, gbuffer0_handle, ssao_factor_handle,
             ssao_noise_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                builder.ReadTexture(depth_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
                builder.ReadTexture(gbuffer0_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
                builder.ReadTexture(ssao_noise_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
                builder.WriteTexture(ssao_factor_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
            },
            // Execute: Run SSAO compute shader
            [this, depth_handle, gbuffer0_handle, ssao_factor_handle,
             ssao_noise_handle](CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder) {
                cl->BeginComputePass("SSAO Pass");
                ssao->ssao_pass->SetResource(builder.GetTexture(depth_handle), "depth_tex");
                ssao->ssao_pass->SetResource(builder.GetTexture(gbuffer0_handle), "normal_tex");
                ssao->ssao_pass->SetResource(sampler, "default_sampler");
                ssao->ssao_pass->SetResource(builder.GetTexture(ssao_factor_handle), "ao_factor_tex");
                ssao->ssao_pass->SetResource(ssao->ssao_constants_buffer, "SSAOConstant_cb");
                ssao->ssao_pass->SetResource(builder.GetTexture(ssao_noise_handle), "ssao_noise_tex");
                cl->BindPipeline(ssao->ssao_pass);
                cl->Dispatch(AlignUp<u32>(width, 8), AlignUp<u32>(height, 8), 1);
                cl->EndComputePass();
            });

        // SSAO Blur Pass
        frame_graph->AddPass(
            "SSAO Blur Pass",
            // Setup: Declare resource states
            [ssao_factor_handle, ssao_blur_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                builder.ReadTexture(ssao_factor_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
                builder.WriteTexture(ssao_blur_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
            },
            // Execute: Run SSAO blur compute shader
            [this, ssao_factor_handle, ssao_blur_handle](CommandList *cl,
                                                         Horizon::Backend::FrameGraphBuilder &builder) {
                cl->BeginComputePass("SSAO Blur Pass");
                ssao->ssao_blur_pass->SetResource(builder.GetTexture(ssao_factor_handle), "ssao_blur_in");
                ssao->ssao_blur_pass->SetResource(builder.GetTexture(ssao_blur_handle), "ssao_blur_out");
                cl->BindPipeline(ssao->ssao_blur_pass);
                cl->Dispatch(AlignUp<u32>(width, 8), AlignUp<u32>(height, 8), 1);
                cl->EndComputePass();
            });

        // Deferred Shading Pass
        frame_graph->AddPass(
            "Deferred Shading Pass",
            // Setup: Declare resource states
            [gbuffer0_handle, gbuffer1_handle, gbuffer2_handle, gbuffer3_handle, depth_handle, shading_color_handle,
             ssao_blur_handle, brdf_lut_handle, prefiltered_env_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                builder.ReadTexture(gbuffer0_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
                builder.ReadTexture(gbuffer1_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
                builder.ReadTexture(gbuffer2_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
                builder.ReadTexture(gbuffer3_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
                builder.ReadTexture(depth_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
                builder.ReadTexture(ssao_blur_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
                builder.ReadTexture(brdf_lut_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
                builder.ReadTexture(prefiltered_env_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
                builder.WriteTexture(shading_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
            },
            // Execute: Run deferred shading compute shader
            [this, gbuffer0_handle, gbuffer1_handle, gbuffer2_handle, gbuffer3_handle, depth_handle,
             shading_color_handle, ssao_blur_handle, brdf_lut_handle,
             prefiltered_env_handle](CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder) {
                cl->BeginComputePass("Deferred Shading Pass");
                deferred->shading_pass->SetResource(builder.GetTexture(gbuffer0_handle), "gbuffer0_tex");
                deferred->shading_pass->SetResource(builder.GetTexture(gbuffer1_handle), "gbuffer1_tex");
                deferred->shading_pass->SetResource(builder.GetTexture(gbuffer2_handle), "gbuffer2_tex");
                deferred->shading_pass->SetResource(builder.GetTexture(gbuffer3_handle), "gbuffer3_tex");
                deferred->shading_pass->SetResource(builder.GetTexture(depth_handle), "depth_tex");
                deferred->shading_pass->SetResource(deferred->deferred_shading_constants_buffer,
                                                    "DeferredShadingConstants_cb");
                deferred->shading_pass->SetResource(scene->m_scene_manager->GetLightCountBuffer(), "LightCountUb_cb");
                deferred->shading_pass->SetResource(scene->m_scene_manager->GetLightParamBuffer(), "LightDataUb_cb");
                deferred->shading_pass->SetResource(builder.GetTexture(shading_color_handle), "out_color");
                deferred->shading_pass->SetResource(builder.GetTexture(ssao_blur_handle), "ao_tex");
                deferred->shading_pass->SetResource(deferred->diffuse_irradiance_sh3_buffer, "DiffuseIrradianceSH3_cb");
                deferred->shading_pass->SetResource(builder.GetTexture(prefiltered_env_handle), "specular_map");
                deferred->shading_pass->SetResource(builder.GetTexture(brdf_lut_handle), "specular_brdf_lut");
                deferred->shading_pass->SetResource(deferred->ibl_sampler, "ibl_sampler");
                cl->BindPipeline(deferred->shading_pass);
                cl->Dispatch(AlignUp<u32>(width, 8), AlignUp<u32>(height, 8), 1);
                cl->EndComputePass();
            });

        // Luminance Histogram Pass
        frame_graph->AddPass(
            "Luminance Histogram Pass",
            // Setup: Declare resource states
            [shading_color_handle, histogram_buffer_handle,
             adapted_luminance_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                builder.ReadTexture(shading_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
                builder.WriteBuffer(histogram_buffer_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
                builder.WriteBuffer(adapted_luminance_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
            },
            // Execute: Run luminance histogram compute shader
            [this, shading_color_handle, histogram_buffer_handle,
             adapted_luminance_handle](CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder) {
                cl->BeginComputePass("Luminance Histogram Pass");
                post_process->auto_exposure_pass->luminance_histogram_pass->SetResource(
                    builder.GetTexture(shading_color_handle), "color_image");
                post_process->auto_exposure_pass->luminance_histogram_pass->SetResource(
                    post_process->auto_exposure_pass->luminance_histogram_constants_buffer,
                    "LuminanceHistogramConstants_cb");
                post_process->auto_exposure_pass->luminance_histogram_pass->SetResource(
                    builder.GetBuffer(histogram_buffer_handle), "histogram");
                post_process->auto_exposure_pass->luminance_histogram_pass->SetResource(
                    builder.GetBuffer(adapted_luminance_handle), "adaptedLuminance");
                cl->BindPipeline(post_process->auto_exposure_pass->luminance_histogram_pass);
                cl->Dispatch(AlignUp<u32>(width, 16), AlignUp<u32>(height, 16), 1);
                cl->EndComputePass();
            });

        // Luminance Average Pass
        frame_graph->AddPass(
            "Luminance Average Pass",
            // Setup: Declare resource states
            [histogram_buffer_handle, adapted_luminance_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                builder.ReadBuffer(histogram_buffer_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
                builder.WriteBuffer(adapted_luminance_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
            },
            // Execute: Run luminance average compute shader
            [this, histogram_buffer_handle, adapted_luminance_handle](CommandList *cl,
                                                                      Horizon::Backend::FrameGraphBuilder &builder) {
                cl->BeginComputePass("Luminance Average Pass");
                post_process->auto_exposure_pass->luminance_average_pass->SetResource(
                    post_process->auto_exposure_pass->luminance_histogram_constants_buffer,
                    "LuminanceHistogramConstants_cb");
                post_process->auto_exposure_pass->luminance_average_pass->SetResource(
                    builder.GetBuffer(histogram_buffer_handle), "histogram");
                post_process->auto_exposure_pass->luminance_average_pass->SetResource(
                    builder.GetBuffer(adapted_luminance_handle), "adaptedLuminance");
                cl->BindPipeline(post_process->auto_exposure_pass->luminance_average_pass);
                cl->Dispatch(1, 1, 1);
                cl->EndComputePass();
            });

        // Post Process Pass
        frame_graph->AddPass(
            "Post Process Pass",
            // Setup: Declare resource states
            [shading_color_handle, pp_color_handle,
             adapted_luminance_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                builder.ReadTexture(shading_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
                builder.ReadBuffer(adapted_luminance_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
                builder.WriteTexture(pp_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
            },
            // Execute: Run post process compute shader
            [this, shading_color_handle, pp_color_handle,
             adapted_luminance_handle](CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder) {
                cl->BeginComputePass("Post Process Pass");
                post_process->post_process_pass->SetResource(builder.GetTexture(shading_color_handle), "color_image");
                post_process->post_process_pass->SetResource(builder.GetTexture(pp_color_handle), "out_color_image");
                post_process->post_process_pass->SetResource(builder.GetBuffer(adapted_luminance_handle),
                                                             "adaptedLuminance");
                cl->BindPipeline(post_process->post_process_pass);
                cl->Dispatch(AlignUp<u32>(width, 8), AlignUp<u32>(height, 8), 1);
                cl->EndComputePass();
            });

        // TAA Pass
        frame_graph->AddPass(
            "TAA Pass",
            // Setup: Declare resource states
            [previous_color_handle, pp_color_handle, gbuffer4_handle,
             output_color_handle](Horizon::Backend::FrameGraphBuilder &builder) {
                builder.ReadTexture(previous_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
                builder.ReadTexture(pp_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
                builder.ReadTexture(gbuffer4_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
                builder.WriteTexture(output_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
            },
            // Execute: Run TAA compute shader
            [this, previous_color_handle, pp_color_handle, gbuffer4_handle,
             output_color_handle](CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder) {
                cl->BeginComputePass("TAA Pass");
                antialiasing->taa_pass->SetResource(builder.GetTexture(previous_color_handle), "prev_color_tex");
                antialiasing->taa_pass->SetResource(builder.GetTexture(pp_color_handle), "curr_color_tex");
                antialiasing->taa_pass->SetResource(builder.GetTexture(gbuffer4_handle), "mv_tex");
                antialiasing->taa_pass->SetResource(builder.GetTexture(output_color_handle), "out_color_tex");
                cl->BindPipeline(antialiasing->taa_pass);
                cl->Dispatch(AlignUp<u32>(width, 8), AlignUp<u32>(height, 8), 1);
                cl->EndComputePass();
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