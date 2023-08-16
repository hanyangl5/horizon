#include "horizon_pipeline.h"


void HorizonPipeline::InitAPI() { rhi = renderer->GetRhi(); }

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

    deferred = std::make_unique<DeferredData>(rhi);
    ssao = std::make_unique<SSAOData>(rhi);
    scene = std::make_unique<SceneData>(renderer->GetSceneManager(), rhi);
}

void HorizonPipeline::UpdatePipelineResources() {

    auto cam = scene->scene_camera;

    // taa jitter

    auto view = cam->GetViewMatrix();
    auto proj = cam->GetProjectionMatrix();

    auto vp = view * proj;
    auto inverse_vp = vp.Invert();

    scene->m_scene_manager->camera_ub.prev_vp = scene->m_scene_manager->camera_ub.vp;
    scene->m_scene_manager->camera_ub.vp = vp;

    scene->m_scene_manager->camera_ub.camera_pos = cam->GetPosition();
    scene->m_scene_manager->camera_ub.ev100 = cam->GetEv100();

    deferred->deferred_shading_constants.camera_pos = Math::float4(cam->GetPosition());
    deferred->deferred_shading_constants.inverse_vp = inverse_vp;

    ssao->ao_constansts.proj = proj;
    ssao->ao_constansts.inv_proj = proj.Invert();
    ssao->ao_constansts.view = view;
    ssao->ao_constansts.noise_scale_x = width / SSAOData::SSAO_NOISE_TEX_WIDTH;
    ssao->ao_constansts.noise_scale_y = height / SSAOData::SSAO_NOISE_TEX_HEIGHT;
}

void HorizonPipeline::run() {

    bool first_frame = true;

    while (!window->ShouldClose()) {
        scene->scene_camera_controller->ProcessInput(window.get());

        rhi->AcquireNextFrame(swap_chain);
        UpdatePipelineResources();
        auto resource_uploaded_semaphore = rhi->CreateSemaphore1();

        // resource upload
        {
            auto transfer = rhi->GetCommandList(CommandQueueType::GRAPHICS);

            transfer->BeginRecording();

            // upload textures, vertex/index buffer
            if (first_frame) {
                scene->m_scene_manager->UploadBuiltInResources(transfer);
                scene->m_scene_manager->UploadMeshResources(transfer);
                //scene->m_scene_manager->UploadDecalResources(transfer);
            }
            // scene data
            scene->m_scene_manager->UploadLightResources(transfer);
            scene->m_scene_manager->UploadCameraResources(transfer);

            // deferred data
            transfer->UpdateBuffer(deferred->deferred_shading_constants_buffer, &deferred->deferred_shading_constants,
                                   sizeof(deferred->deferred_shading_constants));
            // ssao data
            transfer->UpdateBuffer(ssao->ssao_constants_buffer, &ssao->ao_constansts, sizeof(SSAOData::AOConstant));
            if (first_frame) {

                {
                    TextureUpdateDesc desc{};
                    desc.texture_data_desc = &ssao->ssao_noise_tex_data_desc;
                    desc.size = GetBytesFromTextureFormat(ssao->ssao_noise_tex->m_format) *
                                SSAOData::SSAO_NOISE_TEX_WIDTH * SSAOData::SSAO_NOISE_TEX_HEIGHT; //
                    transfer->UpdateTexture(ssao->ssao_noise_tex, desc);
                }
                {
                    TextureUpdateDesc desc{};
                    desc.texture_data_desc = &ssao->hbao_noise_tex_data_desc;
                    desc.size = GetBytesFromTextureFormat(ssao->hbao_noise_tex->m_format) *
                                SSAOData::HBAO_RAND_TEX_WIDTH * SSAOData::HBAO_RAND_TEX_HEIGHT; //
                    transfer->UpdateTexture(ssao->hbao_noise_tex, desc);
                }
            }

            BarrierDesc barrier{};
            TextureBarrierDesc tb;
            // pass resources
            tb.src_state = ResourceState::RESOURCE_STATE_UNDEFINED;
            tb.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;

            tb.texture = deferred->shading_color_image;
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
                tb.texture = ssao->hbao_noise_tex;
                barrier.texture_memory_barriers.push_back(tb);
            } else {
                //tb.texture = antialiasing->previous_color_texture;
                //tb.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                //tb.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                //barrier.texture_memory_barriers.push_back(tb);
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
        geometry_pass_per_frame_ds->SetResource(scene->m_scene_manager->GetCameraBuffer(), "CameraParamsUb");
        geometry_pass_per_frame_ds->SetResource(scene->m_scene_manager->instance_parameter_buffer,
                                                "instance_parameter");
        geometry_pass_per_frame_ds->Update();

        

        std::vector<Texture *> material_textures;

        for (auto &tex : scene->m_scene_manager->material_textures) {
            material_textures.push_back(tex);
        }

        std::vector<Buffer *> veretx_buffers;
        for (auto &vb : scene->m_scene_manager->vertex_buffers) {
            veretx_buffers.push_back(vb);
        }

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

                tb.dst_state = ResourceState::RESOURCE_STATE_DEPTH_WRITE;
                tb.texture = deferred->depth->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                cl->InsertBarrier(image_barriers);
            }

            RenderPassBeginInfo begin_info{};
            begin_info.render_target_count = 1;
            begin_info.render_area = Rect{0, 0, width, height};
            begin_info.render_targets[0].data = deferred->gbuffer0;
            begin_info.render_targets[0].clear_color = {};
            begin_info.render_targets[0].load_op = RenderTargetLoadOp::CLEAR;
            begin_info.render_targets[0].store_op = RenderTargetStoreOp::STORE;

            begin_info.depth_stencil.data = deferred->depth;
            begin_info.depth_stencil.clear_color = ClearValueDepthStencil{1.0, 0};
            begin_info.depth_stencil.load_op = RenderTargetLoadOp::CLEAR;
            begin_info.depth_stencil.store_op = RenderTargetStoreOp::STORE;
            cl->BindDescriptorSets(deferred->geometry_pass, geometry_pass_per_frame_ds);

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
                                                 sizeof(DrawIndexedInstancedCommand) * mesh.draw_offset,
                                                 mesh.draw_count, sizeof(DrawIndexedInstancedCommand));
            }

            cl->EndRenderPass();
            {
                BarrierDesc barrier1{};
                TextureBarrierDesc tb1{};
                tb1.texture = deferred->depth->GetTexture();
                tb1.src_state = ResourceState::RESOURCE_STATE_DEPTH_WRITE;
                tb1.dst_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
                barrier1.texture_memory_barriers.push_back(std::move(tb1));
                cl->InsertBarrier(barrier1);

                BarrierDesc barrier2{};
                tb1.texture = deferred->depth->GetTexture();
                tb1.src_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
                tb1.dst_state = ResourceState::RESOURCE_STATE_DEPTH_WRITE;
                barrier2.texture_memory_barriers.push_back(std::move(tb1));

                cl->InsertBarrier(barrier2);
            }

            {
                BarrierDesc barrier{};

                TextureBarrierDesc tb;
                tb.src_state = ResourceState::RESOURCE_STATE_RENDER_TARGET;
                tb.dst_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
                tb.texture = deferred->gbuffer0->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);

                tb.src_state = ResourceState::RESOURCE_STATE_DEPTH_WRITE;
                tb.texture = deferred->depth->GetTexture();
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
                ao_ds->SetResource(ssao->ssao_constants_buffer, "AOConstant");
                ao_ds->SetResource(ssao->ssao_noise_tex, "ssao_noise_tex");
                ao_ds->SetResource(ssao->hbao_noise_tex, "hbao_rand_tex");
                ao_ds->Update();

                compute->BindPipeline(ssao->ssao_pass);
                compute->BindDescriptorSets(ssao->ssao_pass, ao_ds);
                compute->BeginQuery();
                compute->Dispatch(width / 8 + 1, height / 8 + 1, 1);
                compute->EndQuery();
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

                compute->Dispatch(width / 8 + 1, height / 8 + 1, 1);
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
                shading_ds->SetResource(deferred->depth->GetTexture(), "depth_tex");
                shading_ds->SetResource(deferred->shading_color_image, "out_color");
                shading_ds->SetResource(ssao->ssao_blur_image, "ao_tex");
                shading_ds->SetResource(deferred->deferred_shading_constants_buffer, "DeferredShadingConstants");
                shading_ds->Update();

                compute->BindPipeline(deferred->shading_pass);
                compute->BindDescriptorSets(deferred->shading_pass, shading_ds);
                compute->Dispatch(width / 8 + 1, height / 8 + 1, 1);
            }

            {
                BarrierDesc color_image_barrier{};

                TextureBarrierDesc tb;
                tb.src_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                tb.dst_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
                tb.texture = deferred->shading_color_image;
                color_image_barrier.texture_memory_barriers.push_back(tb);

                tb.src_state = ResourceState::RESOURCE_STATE_UNDEFINED;
                tb.dst_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                tb.texture = swap_chain->GetRenderTarget()->GetTexture();
                color_image_barrier.texture_memory_barriers.push_back(tb);

                compute->InsertBarrier(color_image_barrier);
            }

            compute->CopyTexture(deferred->shading_color_image, swap_chain->GetRenderTarget()->GetTexture());

            {
                {
                    BarrierDesc pp_image_barrier{};
                    {
                        TextureBarrierDesc tb;
                        tb.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                        tb.dst_state = ResourceState::RESOURCE_STATE_PRESENT;
                        tb.texture = swap_chain->GetRenderTarget()->GetTexture();
                        pp_image_barrier.texture_memory_barriers.push_back(tb);
                    }

                    compute->InsertBarrier(pp_image_barrier);
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
        rhi->DestroySemaphore(gp_semaphore);
        rhi->DestroySemaphore(resource_uploaded_semaphore);
        if (first_frame) {
            first_frame = false;
        }
        std::string ssao_time = std::string(std::to_string(rhi->QueryResult() / 1000000.0)) + " ms";

        window->UpdateWindowTitle(ssao_time.c_str());
        // Horizon::RDC::EndFrameCapture();
    }

    LOG_INFO("draw done");
}
