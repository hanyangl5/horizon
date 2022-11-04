#include "horizon_pipeline.h"

void HorizonPipeline::InitAPI() {
    rhi = engine->m_render_system->GetRhi();
    m_camera = Memory::MakeUnique<Camera>(Math::float3(0.0, 0.0, 10.0_m), Math::float3(0.0, 0.0, 0.0),
                                        Math::float3(0.0, 1.0_m, 0.0));
    m_camera->SetCameraSpeed(0.1);
    m_camera->SetExposure(16.0f, 1 / 125.0f, 100.0f);
    engine->m_render_system->SetCamera(m_camera.get());
    engine->m_input_system->SetCamera(engine->m_render_system->GetDebugCamera());
}

void HorizonPipeline::InitResources() { InitPipelineResources(); }

void HorizonPipeline::InitPipelineResources() {

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

    deferred = Memory::MakeUnique<DeferredData>(rhi);
    ssao = Memory::MakeUnique<SSAOData>(rhi);
    post_process = Memory::MakeUnique<PostProcessData>(rhi);
    scene = Memory::MakeUnique<SceneData>(engine->m_render_system->GetSceneManager(), rhi,
                                        engine->m_render_system->GetDebugCamera());
}

void HorizonPipeline::UpdatePipelineResources() {

    auto cam = scene->cam;
    scene->camera_ub.vp = cam->GetViewProjectionMatrix();
    scene->camera_ub.camera_pos = cam->GetPosition();
    scene->camera_ub.ev100 = cam->GetEv100();

    post_process->exposure_constants.exposure_ev100__ = Math::float4(cam->GetExposure(), cam->GetEv100(), 0.0, 0.0);

    deferred->deferred_shading_constants.camera_pos = Math::float4(cam->GetPosition());
    deferred->deferred_shading_constants.inverse_vp = cam->GetInvViewProjectionMatrix();

    ssao->ssao_constansts.p = cam->GetProjectionMatrix();
    ssao->ssao_constansts.inv_p = cam->GetInvProjectionMatrix();
    ssao->ssao_constansts.view_mat = cam->GetViewMatrix();
    ssao->ssao_constansts.noise_scale_x = _width / SSAOData::SSAO_NOISE_TEX_WIDTH;
    ssao->ssao_constansts.noise_scale_y = _height / SSAOData::SSAO_NOISE_TEX_HEIGHT;
}

void HorizonPipeline::run() {

    bool first_frame = true;

    while (!engine->m_window->ShouldClose()) {

        engine->m_input_system->Tick();

        rhi->AcquireNextFrame(swap_chain);
        UpdatePipelineResources();
        auto resource_uploaded_semaphore = rhi->CreateSemaphore1();

        // resource upload
        {
            auto transfer = rhi->GetCommandList(CommandQueueType::GRAPHICS);

            transfer->BeginRecording();

            // upload textures, vertex/index buffer
            if (first_frame) {
                scene->m_scene_manager->UploadMeshResources(transfer);
            }
            // scene data

            transfer->UpdateBuffer(scene->light_buffer, scene->lights_param_buffer.data(),
                                   scene->lights_param_buffer.size() * sizeof(LightParams));
            transfer->UpdateBuffer(scene->light_count_buffer, &scene->light_count,
                                   sizeof(scene->light_count) * 4);
            transfer->UpdateBuffer(scene->camera_buffer, &scene->camera_ub, sizeof(SceneData::CameraUb));

            // deferred data
            transfer->UpdateBuffer(deferred->deferred_shading_constants_buffer,
                                   &deferred->deferred_shading_constants, sizeof(deferred->deferred_shading_constants));
            // post process data
            transfer->UpdateBuffer(post_process->exposure_constants_buffer, &post_process->exposure_constants,
                                   sizeof(PostProcessData::ExposureConstant));
            transfer->UpdateBuffer(ssao->ssao_constants_buffer, &ssao->ssao_constansts,
                                   sizeof(SSAOData::SSAOConstant));
            if (first_frame) {

                transfer->UpdateBuffer(deferred->diffuse_irradiance_sh3_buffer,
                                       &deferred->diffuse_irradiance_sh3_constants,
                                       sizeof(deferred->diffuse_irradiance_sh3_constants));
                {
                    TextureUpdateDesc desc{};
                    desc.texture_data_desc = &ssao->ssao_noise_tex_data_desc;
                    desc.size = GetBytesFromTextureFormat(ssao->ssao_noise_tex->m_format) *
                                SSAOData::SSAO_NOISE_TEX_WIDTH * SSAOData::SSAO_NOISE_TEX_HEIGHT; //
                    transfer->UpdateTexture(ssao->ssao_noise_tex, desc);
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
                    transfer->UpdateTexture(deferred->prefiltered_irradiance_env_map, desc2);
                }
                {
                    TextureUpdateDesc desc2{};
                    desc2.first_layer = 0;
                    desc2.layer_count = 1;
                    desc2.first_mip_level = 0;
                    desc2.mip_level_count = 1;
                    desc2.size = sizeof(char) * deferred->brdf_lut_data_desc.raw_data.size();
                    desc2.texture_data_desc = &deferred->brdf_lut_data_desc;
                    transfer->UpdateTexture(deferred->brdf_lut, desc2);
                }
            }

            BarrierDesc barrier{};
            TextureBarrierDesc tb;
            // pass resources
            tb.src_state = ResourceState::RESOURCE_STATE_UNDEFINED;
            tb.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;

            tb.texture = deferred->shading_color_image;
            barrier.texture_memory_barriers.push_back(tb);
            tb.texture = post_process->pp_color_image;
            barrier.texture_memory_barriers.push_back(tb);
            tb.texture = ssao->ssao_factor_image;
            barrier.texture_memory_barriers.push_back(tb);
            tb.texture = ssao->ssao_blur_image;
            barrier.texture_memory_barriers.push_back(tb);

            // pass constants
            if (first_frame) {
                tb.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                tb.dst_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
                tb.texture = ssao->ssao_noise_tex;
                barrier.texture_memory_barriers.push_back(tb);

                tb.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                tb.dst_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
                // brdf lut
                tb.texture = deferred->brdf_lut;
                barrier.texture_memory_barriers.push_back(tb);
                // pfem
                tb.texture = deferred->prefiltered_irradiance_env_map;
                tb.first_layer = 0;
                tb.first_mip_level = 0;
                tb.layer_count = 6;
                tb.mip_level_count = deferred->prefilered_irradiance_env_map_data.mipmap_count;
                barrier.texture_memory_barriers.push_back(tb);
            }
            transfer->InsertBarrier(barrier);

            transfer->EndRecording();

            {
                QueueSubmitInfo submit_info{};
                submit_info.queue_type = CommandQueueType::GRAPHICS;
                submit_info.command_lists.push_back(transfer);
                submit_info.signal_semaphores.push_back(resource_uploaded_semaphore);
                rhi->SubmitCommandLists(submit_info);
            }
        }
        auto geometry_pass_per_frame_ds = deferred->geometry_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);

        // perframe descriptor set
        geometry_pass_per_frame_ds->SetResource(scene->camera_buffer, "CameraParamsUb");
        geometry_pass_per_frame_ds->SetResource(scene->m_scene_manager->draw_parameter_buffer, "per_draw_data");
        geometry_pass_per_frame_ds->SetResource(scene->m_scene_manager->material_description_buffer,
                                                "material_descriptions");
        geometry_pass_per_frame_ds->SetResource(sampler, "default_sampler");
        geometry_pass_per_frame_ds->Update();

        auto geometry_pass_bindless_ds = deferred->geometry_pass->GetDescriptorSet(ResourceUpdateFrequency::BINDLESS);

        auto stack_memory = Memory::GetStackMemoryResource(4096);

        Container::Array<Texture *> material_textures(&stack_memory);

        for (auto &tex : scene->m_scene_manager->material_textures) {
            material_textures.push_back(tex);
        }

        Container::Array<Buffer *> veretx_buffers(&stack_memory);
        for (auto &vb : scene->m_scene_manager->vertex_buffers) {
            veretx_buffers.push_back(vb);
        }

        geometry_pass_bindless_ds->SetBindlessResource(material_textures, "material_textures");
        //geometry_pass_bindless_ds->SetBindlessResource(veretx_buffers, "vertex_buffers");
        geometry_pass_bindless_ds->Update();
        // geometry pass

        auto gp_semaphore = rhi->CreateSemaphore1();
        {
            auto cl = rhi->GetCommandList(CommandQueueType::GRAPHICS);
            cl->BeginRecording();

            {
                BarrierDesc image_barriers{};

                TextureBarrierDesc tb;
                tb.src_state = ResourceState::RESOURCE_STATE_UNDEFINED;
                tb.dst_state = ResourceState::RESOURCE_STATE_RENDER_TARGET;
                tb.texture = deferred->gbuffer0->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                tb.texture = deferred->gbuffer1->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                tb.texture = deferred->gbuffer2->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                tb.texture = deferred->gbuffer3->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                tb.texture = deferred->vbuffer0->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                tb.dst_state = ResourceState::RESOURCE_STATE_DEPTH_WRITE;
                tb.texture = deferred->depth->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                cl->InsertBarrier(image_barriers);
            }

            RenderPassBeginInfo begin_info{};
            begin_info.render_area = Rect{0, 0, _width, _height};
            begin_info.render_targets[0].data = deferred->gbuffer0;
            begin_info.render_targets[0].clear_color = {};
            begin_info.render_targets[1].data = deferred->gbuffer1;
            begin_info.render_targets[1].clear_color = {};
            begin_info.render_targets[2].data = deferred->gbuffer2;
            begin_info.render_targets[2].clear_color = {};
            begin_info.render_targets[3].data = deferred->gbuffer3;
            begin_info.render_targets[3].clear_color = {};
            begin_info.render_targets[4].data = deferred->vbuffer0;
            begin_info.render_targets[4].clear_color = {};
            begin_info.depth_stencil.data = deferred->depth;
            begin_info.depth_stencil.clear_color = ClearValueDepthStencil{1.0, 0};

            cl->BindDescriptorSets(deferred->geometry_pass, geometry_pass_per_frame_ds);
            cl->BindDescriptorSets(deferred->geometry_pass, geometry_pass_bindless_ds);

            cl->BeginRenderPass(begin_info);

            cl->BindPipeline(deferred->geometry_pass);

            u32 draw_offset = 0;

            for (u32 mesh_data = 0; mesh_data < scene->m_scene_manager->mesh_data.size(); mesh_data++) {
                auto &mesh = scene->m_scene_manager->mesh_data[mesh_data];
                auto ib = scene->m_scene_manager->index_buffers[mesh.index_buffer_offset];
                auto vb = scene->m_scene_manager->vertex_buffers[mesh.vertex_buffer_offset];
                u32 offset = 0;
                cl->BindVertexBuffers(1, &vb, &offset);
                cl->BindIndexBuffer(ib, 0);
                cl->BindPushConstant(deferred->geometry_pass, "DrawRootConstant", &mesh.draw_offset);

                cl->DrawIndirectIndexedInstanced(scene->m_scene_manager->indirect_draw_command_buffer1,
                                                 sizeof(IndirectDrawCommand) * mesh.draw_offset, mesh.draw_count,
                                                 sizeof(IndirectDrawCommand));
            }

            cl->EndRenderPass();

            {
                BarrierDesc barrier{};

                TextureBarrierDesc tb;
                tb.src_state = ResourceState::RESOURCE_STATE_RENDER_TARGET;
                tb.dst_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
                tb.texture = deferred->gbuffer0->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);
                tb.texture = deferred->gbuffer1->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);
                tb.texture = deferred->gbuffer2->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);
                tb.texture = deferred->gbuffer3->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);
                tb.src_state = ResourceState::RESOURCE_STATE_DEPTH_WRITE;
                tb.texture = deferred->depth->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);
                tb.src_state = ResourceState::RESOURCE_STATE_RENDER_TARGET;
                tb.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                tb.texture = deferred->vbuffer0->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);

                cl->InsertBarrier(barrier);
            }

            cl->EndRecording();

            {
                QueueSubmitInfo submit_info{};
                submit_info.queue_type = CommandQueueType::GRAPHICS;
                submit_info.command_lists.push_back(cl);
                submit_info.wait_semaphores.push_back(resource_uploaded_semaphore);
                submit_info.signal_semaphores.push_back(gp_semaphore);
                submit_info.wait_image_acquired = true;
                rhi->SubmitCommandLists(submit_info);
            }
        }

        // all following pass use compute queue
        {
            auto compute = rhi->GetCommandList(CommandQueueType::COMPUTE);
            compute->BeginRecording();

            auto ao_ds = ssao->ssao_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);
            // ao pass
            {
                ao_ds->SetResource(deferred->depth->GetTexture(), "depth_tex");
                ao_ds->SetResource(deferred->gbuffer0->GetTexture(), "normal_tex");
                ao_ds->SetResource(sampler, "default_sampler");
                ao_ds->SetResource(ssao->ssao_factor_image, "ao_factor_tex");
                ao_ds->SetResource(ssao->ssao_constants_buffer, "SSAOConstant");
                ao_ds->SetResource(ssao->ssao_noise_tex, "ssao_noise_tex");
                ao_ds->Update();

                compute->BindPipeline(ssao->ssao_pass);
                compute->BindDescriptorSets(ssao->ssao_pass, ao_ds);

                compute->Dispatch(_width / 8 + 1, _height / 8 + 1, 1);
            }

            {
                BarrierDesc barrier{};

                TextureBarrierDesc tb1;
                tb1.src_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                tb1.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                tb1.texture = ssao->ssao_factor_image;
                barrier.texture_memory_barriers.push_back(tb1);
                compute->InsertBarrier(barrier);
            }

            auto ao_blur_ds = ssao->ssao_blur_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);

            {
                ao_blur_ds->SetResource(ssao->ssao_factor_image, "ssao_blur_in");
                ao_blur_ds->SetResource(ssao->ssao_blur_image, "ssao_blur_out");

                ao_blur_ds->Update();

                compute->BindPipeline(ssao->ssao_blur_pass);
                compute->BindDescriptorSets(ssao->ssao_blur_pass, ao_blur_ds);

                compute->Dispatch(_width / 8 + 1, _height / 8 + 1, 1);
            }
            {
                BarrierDesc barrier{};

                TextureBarrierDesc tb1;
                tb1.src_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                tb1.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                tb1.texture = ssao->ssao_blur_image;
                barrier.texture_memory_barriers.push_back(tb1);
                compute->InsertBarrier(barrier);
            }

            auto shading_ds = deferred->shading_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);
            // shading pass
            {

                shading_ds->SetResource(deferred->gbuffer0->GetTexture(), "gbuffer0_tex");
                shading_ds->SetResource(deferred->gbuffer1->GetTexture(), "gbuffer1_tex");
                shading_ds->SetResource(deferred->gbuffer2->GetTexture(), "gbuffer2_tex");
                shading_ds->SetResource(deferred->gbuffer3->GetTexture(), "gbuffer3_tex");
                shading_ds->SetResource(deferred->depth->GetTexture(), "depth_tex");
                shading_ds->SetResource(sampler, "default_sampler");
                shading_ds->SetResource(deferred->deferred_shading_constants_buffer, "DeferredShadingConstants");
                shading_ds->SetResource(scene->light_count_buffer, "LightCountUb");
                shading_ds->SetResource(scene->light_buffer, "LightDataUb");
                shading_ds->SetResource(deferred->shading_color_image, "out_color");
                shading_ds->SetResource(ssao->ssao_blur_image, "ao_tex");
                shading_ds->SetResource(deferred->diffuse_irradiance_sh3_buffer, "DiffuseIrradianceSH3");
                shading_ds->SetResource(deferred->prefiltered_irradiance_env_map, "specular_map");
                shading_ds->SetResource(deferred->brdf_lut, "specular_brdf_lut");
                shading_ds->SetResource(deferred->ibl_sampler, "ibl_sampler");
                shading_ds->Update();

                compute->BindPipeline(deferred->shading_pass);
                compute->BindDescriptorSets(deferred->shading_pass, shading_ds);
                compute->Dispatch(_width / 8 + 1, _height / 8 + 1, 1);
            }

            {
                BarrierDesc color_image_barrier{};

                TextureBarrierDesc tb;
                tb.src_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                tb.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                tb.texture = deferred->shading_color_image;
                color_image_barrier.texture_memory_barriers.push_back(tb);

                compute->InsertBarrier(color_image_barrier);
            }

            auto pp_ds = post_process->post_process_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);
            {
                pp_ds->SetResource(deferred->shading_color_image, "color_image");
                pp_ds->SetResource(post_process->pp_color_image, "out_color_image");
                pp_ds->SetResource(post_process->exposure_constants_buffer, "exposure_constants");
                pp_ds->SetResource(deferred->vbuffer0->GetTexture(), "vbuffer0");

                pp_ds->Update();

                compute->BindPipeline(post_process->post_process_pass);
                compute->BindDescriptorSets(post_process->post_process_pass, pp_ds);
                compute->Dispatch(_width / 8 + 1, _height / 8 + 1, 1);
            }

            {
                {
                    BarrierDesc pp_image_barrier{};

                    TextureBarrierDesc tb;
                    tb.src_state = ResourceState::RESOURCE_STATE_UNDEFINED;
                    tb.dst_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                    tb.texture = swap_chain->GetRenderTarget()->GetTexture();
                    pp_image_barrier.texture_memory_barriers.push_back(tb);

                    TextureBarrierDesc tb2;
                    tb2.src_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                    tb2.dst_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
                    tb2.texture = post_process->pp_color_image;
                    pp_image_barrier.texture_memory_barriers.push_back(tb2);

                    compute->InsertBarrier(pp_image_barrier);
                }

                compute->CopyTexture(post_process->pp_color_image, swap_chain->GetRenderTarget()->GetTexture());

                {
                    BarrierDesc swap_chain_image_barrier{};

                    TextureBarrierDesc tb;
                    tb.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                    tb.dst_state = ResourceState::RESOURCE_STATE_PRESENT;
                    tb.texture = swap_chain->GetRenderTarget()->GetTexture();
                    swap_chain_image_barrier.texture_memory_barriers.push_back(tb);

                    compute->InsertBarrier(swap_chain_image_barrier);
                }
            }
            compute->EndRecording();
            {
                QueueSubmitInfo submit_info{};
                submit_info.queue_type = CommandQueueType::COMPUTE;
                submit_info.command_lists.push_back(compute);
                submit_info.signal_render_complete = true;
                submit_info.wait_semaphores.push_back(gp_semaphore);
                rhi->SubmitCommandLists(submit_info);
            }
        }

        // present
        {
            QueuePresentInfo opaque_pass_ci{};

            opaque_pass_ci.swap_chain = swap_chain;
            rhi->Present(opaque_pass_ci);
        }

        rhi->WaitGpuExecution(CommandQueueType::GRAPHICS);
        rhi->WaitGpuExecution(CommandQueueType::COMPUTE);
        rhi->WaitGpuExecution(CommandQueueType::TRANSFER);

        if (first_frame) {
            first_frame = false;
        }
        // Horizon::RDC::EndFrameCapture();
    }

    LOG_INFO("draw done");
}
