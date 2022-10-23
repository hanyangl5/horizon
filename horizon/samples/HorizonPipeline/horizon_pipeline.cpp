#include "horizon_pipeline.h"

void HorizonPipeline::InitAPI() {
    rhi = engine->m_render_system->GetRhi();

    m_camera = std::make_unique<Camera>(Math::float3(0.0, 0.0, 10.0_m), Math::float3(0.0, 0.0, 0.0),
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

        sampler = rhi->GetSampler(sampler_desc);
    }

    deferred = std::make_unique<DeferredData>(rhi);
    ssao = std::make_unique<SSAOData>(rhi);
    post_process = std::make_unique<PostProcessData>(rhi);
    scene = std::make_unique<SceneData>(rhi, engine->m_render_system->GetDebugCamera());
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

    for (;;) {

        engine->m_input_system->Tick();

        rhi->AcquireNextFrame(swap_chain.get());
        UpdatePipelineResources();
        auto resource_uploaded_semaphore = rhi->GetSemaphore();

        // resource upload
        {
            auto transfer = rhi->GetCommandList(CommandQueueType::GRAPHICS);

            transfer->BeginRecording();

            // upload textures, vertex/index buffer
            if (first_frame) {

                // upload mesh
                for (auto &mesh : scene->meshes) {
                    mesh->UploadResources(transfer);
                }
                BarrierDesc barrier1{};

                for (auto &mesh : scene->meshes) {
                    for (auto &mat : mesh->GetMaterials()) {
                        for (auto &[type, texture] : mat.material_textures) {
                            TextureBarrierDesc mip_map_barrier{};
                            mip_map_barrier.texture = texture.texture.get();
                            mip_map_barrier.first_mip_level = 0;
                            mip_map_barrier.mip_level_count = 1;
                            mip_map_barrier.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                            mip_map_barrier.dst_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
                            barrier1.texture_memory_barriers.emplace_back(mip_map_barrier);
                        }
                    }
                }

                transfer->InsertBarrier(barrier1);

                for (auto &mesh : scene->meshes) {
                    for (auto &mat : mesh->GetMaterials()) {
                        for (auto &[type, texture] : mat.material_textures) {
                            transfer->GenerateMipMap(texture.texture.get(), true);
                        }
                    }
                }
                BarrierDesc barrier2{};
                for (auto &mesh : scene->meshes) {
                    for (auto &mat : mesh->GetMaterials()) {
                        for (auto &[type, texture] : mat.material_textures) {
                            TextureBarrierDesc mip_map_barrier{};
                            mip_map_barrier.texture = texture.texture.get();
                            mip_map_barrier.first_mip_level = 0;
                            mip_map_barrier.mip_level_count = texture.texture->mip_map_level;
                            mip_map_barrier.src_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
                            mip_map_barrier.dst_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
                            barrier2.texture_memory_barriers.emplace_back(mip_map_barrier);
                        }
                    }
                }
                transfer->InsertBarrier(barrier2);
            }

            // scene data
            transfer->UpdateBuffer(scene->light_buffer.get(), scene->lights_param_buffer.data(),
                                   scene->lights_param_buffer.size() * sizeof(LightParams));
            transfer->UpdateBuffer(scene->light_count_buffer.get(), &scene->light_count,
                                   sizeof(scene->light_count) * 4);
            transfer->UpdateBuffer(scene->camera_buffer.get(), &scene->camera_ub, sizeof(SceneData::CameraUb));

            // deferred data
            transfer->UpdateBuffer(deferred->deferred_shading_constants_buffer.get(),
                                   &deferred->deferred_shading_constants, sizeof(deferred->deferred_shading_constants));
            // post process data
            transfer->UpdateBuffer(post_process->exposure_constants_buffer.get(), &post_process->exposure_constants,
                                   sizeof(PostProcessData::ExposureConstant));
            transfer->UpdateBuffer(ssao->ssao_constants_buffer.get(), &ssao->ssao_constansts,
                                   sizeof(SSAOData::SSAOConstant));
            if (first_frame) {

                transfer->UpdateBuffer(deferred->diffuse_irradiance_sh3_buffer.get(),
                                       &deferred->diffuse_irradiance_sh3_constants,
                                       sizeof(deferred->diffuse_irradiance_sh3_constants));
                {
                    TextureUpdateDesc desc{};
                    desc.texture_data_desc = &ssao->ssao_noise_tex_data_desc;
                    desc.size = GetBytesFromTextureFormat(ssao->ssao_noise_tex->m_format) *
                                SSAOData::SSAO_NOISE_TEX_WIDTH * SSAOData::SSAO_NOISE_TEX_HEIGHT; //
                    transfer->UpdateTexture(ssao->ssao_noise_tex.get(), desc);
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
                    transfer->UpdateTexture(deferred->prefiltered_irradiance_env_map.get(), desc2);
                }
                {
                    TextureUpdateDesc desc2{};
                    desc2.first_layer = 0;
                    desc2.layer_count = 1;
                    desc2.first_mip_level = 0;
                    desc2.mip_level_count = 1;
                    desc2.size = sizeof(char) * deferred->brdf_lut_data_desc.raw_data.size();
                    desc2.texture_data_desc = &deferred->brdf_lut_data_desc;
                    transfer->UpdateTexture(deferred->brdf_lut.get(), desc2);
                }
            }


            
            BarrierDesc barrier{};
            TextureBarrierDesc tb;
            // pass resources
            tb.src_state = ResourceState::RESOURCE_STATE_UNDEFINED;
            tb.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;

            tb.texture = deferred->shading_color_image.get();
            barrier.texture_memory_barriers.push_back(tb);
            tb.texture = post_process->pp_color_image.get();
            barrier.texture_memory_barriers.push_back(tb);
            tb.texture = ssao->ssao_factor_image.get();
            barrier.texture_memory_barriers.push_back(tb);
            tb.texture = ssao->ssao_blur_image.get();
            barrier.texture_memory_barriers.push_back(tb);

            // pass constants
            if (first_frame) {
                tb.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                tb.dst_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
                tb.texture = ssao->ssao_noise_tex.get();
                barrier.texture_memory_barriers.push_back(tb);

                tb.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                tb.dst_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
                // brdf lut
                tb.texture = deferred->brdf_lut.get();
                barrier.texture_memory_barriers.push_back(tb);
                // pfem
                tb.texture = deferred->prefiltered_irradiance_env_map.get();
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
                submit_info.signal_semaphores.push_back(resource_uploaded_semaphore.get());
                rhi->SubmitCommandLists(submit_info);
            }
        }

        auto geometry_pass_per_frame_ds = deferred->geometry_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);

        // perframe descriptor set
        geometry_pass_per_frame_ds->SetResource(scene->camera_buffer.get(), "CameraParamsUb");
        geometry_pass_per_frame_ds->Update();

        // material descriptor set
        for (auto &mesh : scene->meshes) {
            for (auto &material : mesh->GetMaterials()) {
                if (material.GetShadingModelID() == ShadingModel::SHADING_MODEL_OPAQUE) {
                    material.material_descriptor_set =
                        deferred->geometry_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_BATCH);
                    material.material_descriptor_set->SetResource(
                        material.material_textures.at(MaterialTextureType::BASE_COLOR).texture.get(),
                        "base_color_texture");
                    material.material_descriptor_set->SetResource(
                        material.material_textures.at(MaterialTextureType::NORMAL).texture.get(), "normal_texture");
                    material.material_descriptor_set->SetResource(
                        material.material_textures.at(MaterialTextureType::METALLIC_ROUGHTNESS).texture.get(),
                        "metallic_roughness_texture");
                    material.material_descriptor_set->SetResource(
                        material.material_textures.at(MaterialTextureType::EMISSIVE).texture.get(), "emissive_texture");

                    material.material_descriptor_set->SetResource(sampler.get(), "default_sampler");
                    material.material_descriptor_set->SetResource(material.param_buffer.get(), "MaterialParamsUb");
                    material.material_descriptor_set->Update();
                } else if (material.GetShadingModelID() == ShadingModel::SHADING_MODEL_MASKED) {
                }
            }
        }
        // geometry pass

        auto gp_semaphore = rhi->GetSemaphore();
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
                tb.dst_state = ResourceState::RESOURCE_STATE_DEPTH_WRITE;
                tb.texture = deferred->depth->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                cl->InsertBarrier(image_barriers);
            }

            RenderPassBeginInfo begin_info{};
            begin_info.render_area = Rect{0, 0, _width, _height};
            begin_info.render_targets[0].data = deferred->gbuffer0.get();
            begin_info.render_targets[0].clear_color = ClearValueColor{Math::float4(0.0)};
            begin_info.render_targets[1].data = deferred->gbuffer1.get();
            begin_info.render_targets[1].clear_color = ClearValueColor{Math::float4(0.0)};
            begin_info.render_targets[2].data = deferred->gbuffer2.get();
            begin_info.render_targets[2].clear_color = ClearValueColor{Math::float4(0.0)};
            begin_info.render_targets[3].data = deferred->gbuffer3.get();
            begin_info.render_targets[3].clear_color = ClearValueColor{Math::float4(0.0)};
            begin_info.depth_stencil.data = deferred->depth.get();
            begin_info.depth_stencil.clear_color = ClearValueDepthStencil{1.0, 0};

            cl->BindDescriptorSets(deferred->geometry_pass, geometry_pass_per_frame_ds);

            cl->BeginRenderPass(begin_info);

            cl->BindPipeline(deferred->geometry_pass);
            for (auto &mesh : scene->meshes) {
                u32 offset = 0;
                auto vb = mesh->GetVertexBuffer();
                cl->BindVertexBuffers(1, &vb, &offset);
                cl->BindIndexBuffer(mesh->GetIndexBuffer(), 0);

                for (auto &node : mesh->GetNodes()) {
                    if (node.mesh_primitives.empty()) {
                        continue;
                    }
                    auto mat = node.GetModelMatrix().Transpose();
                    cl->BindPushConstant(deferred->geometry_pass, "root_constant_model_mat", &mat);

                    for (auto &m : node.mesh_primitives) {
                        auto &material = mesh->GetMaterial(m->material_id);
                        if (material.GetShadingModelID() == ShadingModel::SHADING_MODEL_OPAQUE) {

                            cl->BindDescriptorSets(deferred->geometry_pass, material.material_descriptor_set);
                            cl->DrawIndexedInstanced(m->index_count, m->index_offset, 0);
                        }
                    }
                }
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

       
                cl->InsertBarrier(barrier);
            }

            cl->EndRecording();

            {
                QueueSubmitInfo submit_info{};
                submit_info.queue_type = CommandQueueType::GRAPHICS;
                submit_info.command_lists.push_back(cl);
                submit_info.wait_semaphores.push_back(resource_uploaded_semaphore.get());
                submit_info.signal_semaphores.push_back(gp_semaphore.get());
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
                ao_ds->SetResource(sampler.get(), "default_sampler");
                ao_ds->SetResource(ssao->ssao_factor_image.get(), "ao_factor_tex");
                ao_ds->SetResource(ssao->ssao_constants_buffer.get(), "SSAOConstant");
                ao_ds->SetResource(ssao->ssao_noise_tex.get(), "ssao_noise_tex");
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
                tb1.texture = ssao->ssao_factor_image.get();
                barrier.texture_memory_barriers.push_back(tb1);
                compute->InsertBarrier(barrier);
            }

            auto ao_blur_ds = ssao->ssao_blur_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);

            {
                ao_blur_ds->SetResource(ssao->ssao_factor_image.get(), "ssao_blur_in");
                ao_blur_ds->SetResource(ssao->ssao_blur_image.get(), "ssao_blur_out");

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
                tb1.texture = ssao->ssao_blur_image.get();
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
                shading_ds->SetResource(sampler.get(), "default_sampler");
                shading_ds->SetResource(deferred->deferred_shading_constants_buffer.get(), "DeferredShadingConstants");
                shading_ds->SetResource(scene->light_count_buffer.get(), "LightCountUb");
                shading_ds->SetResource(scene->light_buffer.get(), "LightDataUb");
                shading_ds->SetResource(deferred->shading_color_image.get(), "out_color");
                shading_ds->SetResource(ssao->ssao_blur_image.get(), "ao_tex");
                shading_ds->SetResource(deferred->diffuse_irradiance_sh3_buffer.get(), "DiffuseIrradianceSH3");
                shading_ds->SetResource(deferred->prefiltered_irradiance_env_map.get(), "specular_map");
                shading_ds->SetResource(deferred->brdf_lut.get(), "specular_brdf_lut");
                shading_ds->SetResource(deferred->ibl_sampler.get(), "ibl_sampler");
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
                tb.texture = deferred->shading_color_image.get();
                color_image_barrier.texture_memory_barriers.push_back(tb);

                compute->InsertBarrier(color_image_barrier);
            }

            auto pp_ds = post_process->post_process_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);
            {
                pp_ds->SetResource(deferred->shading_color_image.get(), "color_image");
                pp_ds->SetResource(post_process->pp_color_image.get(), "out_color_image");
                pp_ds->SetResource(post_process->exposure_constants_buffer.get(), "exposure_constants");

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
                    tb2.texture = post_process->pp_color_image.get();
                    pp_image_barrier.texture_memory_barriers.push_back(tb2);

                    compute->InsertBarrier(pp_image_barrier);
                }

                compute->CopyTexture(post_process->pp_color_image.get(), swap_chain->GetRenderTarget()->GetTexture());

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
                submit_info.wait_semaphores.push_back(gp_semaphore.get());
                rhi->SubmitCommandLists(submit_info);
            }
        }

        // present
        {
            QueuePresentInfo opaque_pass_ci{};

            opaque_pass_ci.swap_chain = swap_chain.get();
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
