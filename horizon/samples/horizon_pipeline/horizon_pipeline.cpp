#include "horizon_pipeline.h"

void HorizonPipeline::InitAPI() {
    rhi = engine->m_render_system->GetRhi();
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

    geometry = Memory::MakeUnique<GeometryData>(rhi);
    shading = Memory::MakeUnique<ShadingData>(rhi);
    decal = Memory::MakeUnique<DecalData>(rhi, nullptr, geometry.get());
    ssao = Memory::MakeUnique<SSAOData>(rhi);
    post_process = Memory::MakeUnique<PostProcessData>(rhi);
    antialiasing = Memory::MakeUnique<TAAData>(rhi);
    scene = Memory::MakeUnique<SceneData>(engine->m_render_system->GetSceneManager(), rhi);
}

void HorizonPipeline::UpdatePipelineResources() {

    auto cam = scene->scene_camera;

    // taa jitter
    
    auto &jitter_offset = antialiasing->GetJitterOffset();
    auto &view = cam->GetViewMatrix();
    auto &proj = cam->GetProjectionMatrix();
    f32 offset_x = (jitter_offset.x - 0.5) / _width;
    f32 offset_y = (jitter_offset.y - 0.5) / _height;

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

    scene->m_scene_manager->scene_constants.camera_view = inverse_vp;
    scene->m_scene_manager->scene_constants.camera_projection = proj;
    scene->m_scene_manager->scene_constants.camera_view_projection = vp;
    scene->m_scene_manager->scene_constants.camera_inverse_view_projection = inverse_vp;
    scene->m_scene_manager->scene_constants.camera_pos = cam->GetPosition();
    scene->m_scene_manager->scene_constants.ibl_intensity = 10000.0f;
    scene->m_scene_manager->scene_constants.resolution[0] = _width;
    scene->m_scene_manager->scene_constants.resolution[1] = _height;


    post_process->exposure_constants.exposure_ev100__ = Math::float4(cam->GetExposure(), cam->GetEv100(), 0.0, 0.0);


    ssao->ssao_constansts.inv_proj = proj.Invert();
    ssao->ssao_constansts.noise_scale_x = _width / SSAOData::SSAO_NOISE_TEX_WIDTH;
    ssao->ssao_constansts.noise_scale_y = _height / SSAOData::SSAO_NOISE_TEX_HEIGHT;

}

void HorizonPipeline::run() {

    bool first_frame = true;

    while (!engine->m_window->ShouldClose()) {
        scene->scene_camera_controller->ProcessInput(engine->m_window.get());

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
                scene->m_scene_manager->UploadDecalResources(transfer);
            }
            // scene data
            scene->m_scene_manager->UploadLightResources(transfer);
            scene->m_scene_manager->UploadCameraResources(transfer);

            // deferred data
            transfer->UpdateBuffer(scene->m_scene_manager->scene_constants_buffer,
                                   &scene->m_scene_manager->scene_constants,
                                   sizeof(scene->m_scene_manager->scene_constants));
            // post process data
            transfer->UpdateBuffer(post_process->exposure_constants_buffer, &post_process->exposure_constants,
                                   sizeof(PostProcessData::ExposureConstant));
            transfer->UpdateBuffer(ssao->ssao_constants_buffer, &ssao->ssao_constansts,
                                   sizeof(SSAOData::SSAOConstant));
            transfer->UpdateBuffer(antialiasing->taa_prev_curr_offset_buffer, &antialiasing->taa_prev_curr_offset,
                                   sizeof(TAAData::TAAPrevCurrOffset));
            
            if (first_frame) {

                transfer->UpdateBuffer(shading->diffuse_irradiance_sh3_buffer,
                                       &shading->diffuse_irradiance_sh3_constants,
                                       sizeof(shading->diffuse_irradiance_sh3_constants));
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
                    desc2.mip_level_count = shading->prefiltered_irradiance_env_map->mip_map_level;
                    desc2.size = sizeof(char) * shading->prefilered_irradiance_env_map_data.raw_data.size();
                    desc2.texture_data_desc = &shading->prefilered_irradiance_env_map_data;
                    transfer->UpdateTexture(shading->prefiltered_irradiance_env_map, desc2);
                }
                {
                    TextureUpdateDesc desc2{};
                    desc2.first_layer = 0;
                    desc2.layer_count = 1;
                    desc2.first_mip_level = 0;
                    desc2.mip_level_count = 1;
                    desc2.size = sizeof(char) * shading->brdf_lut_data_desc.raw_data.size();
                    desc2.texture_data_desc = &shading->brdf_lut_data_desc;
                    transfer->UpdateTexture(shading->brdf_lut, desc2);
                }
            }

            BarrierDesc barrier{};
            TextureBarrierDesc tb;
            // pass resources
            tb.src_state = ResourceState::RESOURCE_STATE_UNDEFINED;
            tb.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;

            tb.texture = shading->shading_color_texture;
            barrier.texture_memory_barriers.push_back(tb);
            tb.texture = post_process->pp_color_texture;
            barrier.texture_memory_barriers.push_back(tb);
            tb.texture = ssao->ssao_factor_texture;
            barrier.texture_memory_barriers.push_back(tb);
            tb.texture = ssao->ssao_blur_texture;
            barrier.texture_memory_barriers.push_back(tb);

            tb.texture = antialiasing->output_color_texture;
            barrier.texture_memory_barriers.push_back(tb);


            // pass constants
            if (first_frame) {
                tb.texture = antialiasing->previous_color_texture;
                tb.src_state = ResourceState::RESOURCE_STATE_UNDEFINED;
                tb.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                barrier.texture_memory_barriers.push_back(tb);

                tb.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                tb.dst_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
                tb.texture = ssao->ssao_noise_tex;
                barrier.texture_memory_barriers.push_back(tb);

                tb.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                tb.dst_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
                // brdf lut
                tb.texture = shading->brdf_lut;
                barrier.texture_memory_barriers.push_back(tb);
                // pfem
                tb.texture = shading->prefiltered_irradiance_env_map;
                tb.first_layer = 0;
                tb.first_mip_level = 0;
                tb.layer_count = 6;
                tb.mip_level_count = shading->prefilered_irradiance_env_map_data.mipmap_count;
                barrier.texture_memory_barriers.push_back(tb);
            } else {
                 tb.texture = antialiasing->previous_color_texture;
                 tb.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                 tb.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
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
        auto geometry_pass_per_frame_ds = geometry->geometry_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);

        // perframe descriptor set
        geometry_pass_per_frame_ds->SetResource(scene->m_scene_manager->GetCameraBuffer(), "CameraParamsUb");
        geometry_pass_per_frame_ds->SetResource(scene->m_scene_manager->instance_parameter_buffer, "instance_parameter");
        geometry_pass_per_frame_ds->SetResource(scene->m_scene_manager->material_description_buffer,
                                                "material_descriptions");
        geometry_pass_per_frame_ds->SetResource(sampler, "default_sampler");
        geometry_pass_per_frame_ds->SetResource(antialiasing->taa_prev_curr_offset_buffer, "TAAOffsets");
        geometry_pass_per_frame_ds->Update();

        auto geometry_pass_bindless_ds = geometry->geometry_pass->GetDescriptorSet(ResourceUpdateFrequency::BINDLESS);

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
                tb.texture = geometry->gbuffer0->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                tb.texture = geometry->gbuffer1->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                tb.texture = geometry->gbuffer2->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                tb.texture = geometry->gbuffer3->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                tb.texture = geometry->gbuffer4->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                //tb.texture = geometry->vbuffer0->GetTexture();
                //image_barriers.texture_memory_barriers.push_back(tb);
                tb.dst_state = ResourceState::RESOURCE_STATE_DEPTH_WRITE;
                tb.texture = geometry->depth->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                cl->InsertBarrier(image_barriers);
            }

            RenderPassBeginInfo begin_info{};
            begin_info.render_target_count = 5;
            begin_info.render_area = Rect{0, 0, _width, _height};
            begin_info.render_targets[0].data = geometry->gbuffer0;
            begin_info.render_targets[0].clear_color = {};
            begin_info.render_targets[0].load_op = RenderTargetLoadOp::CLEAR;
            begin_info.render_targets[0].store_op = RenderTargetStoreOp::STORE;
            begin_info.render_targets[1].data = geometry->gbuffer1;
            begin_info.render_targets[1].clear_color = {};
            begin_info.render_targets[1].load_op = RenderTargetLoadOp::CLEAR;
            begin_info.render_targets[1].store_op = RenderTargetStoreOp::STORE;
            begin_info.render_targets[2].data = geometry->gbuffer2;
            begin_info.render_targets[2].clear_color = {};
            begin_info.render_targets[2].load_op = RenderTargetLoadOp::CLEAR;
            begin_info.render_targets[2].store_op = RenderTargetStoreOp::STORE;
            begin_info.render_targets[3].data = geometry->gbuffer3;
            begin_info.render_targets[3].clear_color = {};
            begin_info.render_targets[3].load_op = RenderTargetLoadOp::CLEAR;
            begin_info.render_targets[3].store_op = RenderTargetStoreOp::STORE;
            begin_info.render_targets[4].data = geometry->gbuffer4;
            begin_info.render_targets[4].clear_color = {};
            begin_info.render_targets[4].load_op = RenderTargetLoadOp::CLEAR;
            begin_info.render_targets[4].store_op = RenderTargetStoreOp::STORE;
            begin_info.depth_stencil.data = geometry->depth;
            begin_info.depth_stencil.clear_color = ClearValueDepthStencil{1.0, 0};
            begin_info.depth_stencil.load_op = RenderTargetLoadOp::CLEAR;
            begin_info.depth_stencil.store_op = RenderTargetStoreOp::STORE;
            cl->BindDescriptorSets(geometry->geometry_pass, geometry_pass_per_frame_ds);
            cl->BindDescriptorSets(geometry->geometry_pass, geometry_pass_bindless_ds);

            cl->BeginRenderPass(begin_info);

            cl->BindPipeline(geometry->geometry_pass);

            u32 draw_offset = 0;

            for (u32 mesh_data = 0; mesh_data < scene->m_scene_manager->mesh_data.size(); mesh_data++) {
                auto &mesh = scene->m_scene_manager->mesh_data[mesh_data];
                auto ib = scene->m_scene_manager->index_buffers[mesh.index_buffer_offset];
                auto vb = scene->m_scene_manager->vertex_buffers[mesh.vertex_buffer_offset];
                u32 offset = 0;
                cl->BindVertexBuffers(1, &vb, &offset);
                cl->BindIndexBuffer(ib, 0);
                cl->BindPushConstant(geometry->geometry_pass, "DrawRootConstant", &mesh.draw_offset);

                cl->DrawIndirectIndexedInstanced(scene->m_scene_manager->indirect_draw_command_buffer1,
                                                 sizeof(DrawIndexedInstancedCommand) * mesh.draw_offset,
                                                 mesh.draw_count, sizeof(DrawIndexedInstancedCommand));
            }

            cl->EndRenderPass();
            {
                BarrierDesc barrier1{};
                TextureBarrierDesc tb1{};
                tb1.texture = geometry->depth->GetTexture();
                tb1.src_state = ResourceState::RESOURCE_STATE_DEPTH_WRITE;
                tb1.dst_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
                barrier1.texture_memory_barriers.push_back(std::move(tb1));
                tb1.texture = decal->scene_depth_texture;
                tb1.src_state = ResourceState::RESOURCE_STATE_UNDEFINED;
                tb1.dst_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                barrier1.texture_memory_barriers.push_back(std::move(tb1));
                cl->InsertBarrier(barrier1);

                cl->CopyTexture(geometry->depth->GetTexture(), decal->scene_depth_texture);
                BarrierDesc barrier2{};
                tb1.texture = geometry->depth->GetTexture();
                tb1.src_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
                tb1.dst_state = ResourceState::RESOURCE_STATE_DEPTH_WRITE;
                barrier2.texture_memory_barriers.push_back(std::move(tb1));
                tb1.texture = decal->scene_depth_texture;
                tb1.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                tb1.dst_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
                barrier2.texture_memory_barriers.push_back(std::move(tb1));
                cl->InsertBarrier(barrier2);
            }

            // deferred decal
            {
                auto decal_ds = decal->decal_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);

                decal_ds->SetResource(scene->m_scene_manager->GetCameraBuffer(), "CameraParamsUb");
                decal_ds->SetResource(scene->m_scene_manager->decal_instance_parameter_buffer,
                                      "decal_instance_parameter");
                decal_ds->SetResource(scene->m_scene_manager->decal_material_description_buffer,
                                      "decal_material_descriptions");
                decal_ds->SetResource(sampler, "default_sampler");
                decal_ds->SetResource(decal->scene_depth_texture, "depth_tex");
                decal_ds->SetResource(scene->m_scene_manager->scene_constants_buffer, "SceneConstants");
                decal_ds->Update();

                auto decal_pass_bindless_ds = decal->decal_pass->GetDescriptorSet(ResourceUpdateFrequency::BINDLESS);

                Container::Array<Texture *> decal_material_textures(&stack_memory);

                for (auto &tex : scene->m_scene_manager->decal_material_textures) {
                    decal_material_textures.push_back(tex);
                }

                decal_pass_bindless_ds->SetBindlessResource(decal_material_textures, "decal_material_textures");
                decal_pass_bindless_ds->Update();

                begin_info.render_targets[0].load_op = RenderTargetLoadOp::LOAD;
                begin_info.render_targets[1].load_op = RenderTargetLoadOp::LOAD;
                begin_info.render_targets[2].load_op = RenderTargetLoadOp::LOAD;
                begin_info.render_targets[3].load_op = RenderTargetLoadOp::LOAD;
                begin_info.render_targets[4].load_op = RenderTargetLoadOp::LOAD;
                begin_info.depth_stencil.load_op = RenderTargetLoadOp::LOAD;
                begin_info.depth_stencil.store_op = RenderTargetStoreOp::NONE;

                cl->BeginRenderPass(begin_info);

                auto vb = scene->m_scene_manager->GetUnitCubeVertexBuffer();
                auto ib = scene->m_scene_manager->GetUnitCubeIndexBuffer();
                u32 offset = 0;
                cl->BindVertexBuffers(1, &vb, &offset);
                cl->BindIndexBuffer(ib, 0);
                cl->BindPipeline(decal->decal_pass);
                cl->BindDescriptorSets(decal->decal_pass, decal_ds);
                cl->BindDescriptorSets(decal->decal_pass, decal_pass_bindless_ds);
                cl->DrawIndirectIndexedInstanced(scene->m_scene_manager->decal_indirect_draw_command_buffer1, 0, 1,
                                                 sizeof(DrawIndexedInstancedCommand));
                cl->EndRenderPass();
            }
            {
                BarrierDesc barrier{};

                TextureBarrierDesc tb;
                tb.src_state = ResourceState::RESOURCE_STATE_RENDER_TARGET;
                tb.dst_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
                tb.texture = geometry->gbuffer0->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);
                tb.texture = geometry->gbuffer1->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);
                tb.texture = geometry->gbuffer2->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);
                tb.texture = geometry->gbuffer3->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);

                tb.src_state = ResourceState::RESOURCE_STATE_DEPTH_WRITE;
                tb.texture = geometry->depth->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);
                tb.src_state = ResourceState::RESOURCE_STATE_RENDER_TARGET;
                tb.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                tb.texture = geometry->gbuffer4->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);
                //tb.texture = geometry->vbuffer0->GetTexture();
                //barrier.texture_memory_barriers.push_back(tb);

                cl->InsertBarrier(barrier);
            }
            // shadow
            /*
            {
                for (u32 shadow_map_idx = 0; shadow_map_idx < scene->m_scene_manager->shadow_map_count;
                     shadow_map_idx++) {

                    RenderPassBeginInfo begin_info{};
                    begin_info.render_target_count = 0;
                    begin_info.render_area = Rect{0, 0, _width, _height};
                    begin_info.depth_stencil.data = shadow_map->depth;
                    begin_info.depth_stencil.clear_color = ClearValueDepthStencil{1.0, 0};
                    begin_info.depth_stencil.load_op = RenderTargetLoadOp::CLEAR;
                    begin_info.depth_stencil.store_op = RenderTargetStoreOp::STORE;
                    //cl->BindDescriptorSets(shadow_map->shadow_map_pass, geometry_pass_per_frame_ds);
                    //cl->BindDescriptorSets(shadow_map->shadow_map_pass, geometry_pass_bindless_ds);

                    cl->BeginRenderPass(begin_info);

                    cl->BindPipeline(shadow_map->shadow_map_pass);

                    u32 draw_offset = 0;

                    for (u32 mesh_data = 0; mesh_data < scene->m_scene_manager->mesh_data.size(); mesh_data++) {
                        auto &mesh = scene->m_scene_manager->mesh_data[mesh_data];
                        auto ib = scene->m_scene_manager->index_buffers[mesh.index_buffer_offset];
                        auto vb = scene->m_scene_manager->vertex_buffers[mesh.vertex_buffer_offset];
                        u32 offset = 0;
                        cl->BindVertexBuffers(1, &vb, &offset);
                        cl->BindIndexBuffer(ib, 0);
                        struct ShadowMapDrawRootConstant {
                            u32 shadow_map_idx;
                            u32 mesh_draw_offset;
                        } shadow_map_draw_root_constant;
                        shadow_map_draw_root_constant.shadow_map_idx = shadow_map_idx;
                        shadow_map_draw_root_constant.mesh_draw_offset = mesh.draw_offset;

                        cl->BindPushConstant(geometry->geometry_pass, "ShadowMapDrawRootConstant",
                                             &shadow_map_draw_root_constant);

                        cl->DrawIndirectIndexedInstanced(scene->m_scene_manager->indirect_draw_command_buffer1,
                                                         sizeof(DrawIndexedInstancedCommand) * mesh.draw_offset,
                                                         mesh.draw_count, sizeof(DrawIndexedInstancedCommand));
                    }

                    cl->EndRenderPass();
                }
            }
            */
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

            auto ssao_ds = ssao->ssao_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);
            // ao pass
            {
                ssao_ds->SetResource(geometry->depth->GetTexture(), "depth_tex");
                ssao_ds->SetResource(geometry->gbuffer0->GetTexture(), "normal_tex");
                ssao_ds->SetResource(sampler, "default_sampler");
                ssao_ds->SetResource(ssao->ssao_factor_texture, "ao_factor_tex");
                ssao_ds->SetResource(ssao->ssao_constants_buffer, "SSAOConstant");
                ssao_ds->SetResource(scene->m_scene_manager->scene_constants_buffer, "SceneConstants");
                ssao_ds->SetResource(ssao->ssao_noise_tex, "ssao_noise_tex");
                ssao_ds->Update();

                compute->BindPipeline(ssao->ssao_pass);
                compute->BindDescriptorSets(ssao->ssao_pass, ssao_ds);

                compute->Dispatch(_width / 8 + 1, _height / 8 + 1, 1);
            }

            {
                BarrierDesc barrier{};

                TextureBarrierDesc tb1;
                tb1.src_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                tb1.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                tb1.texture = ssao->ssao_factor_texture;
                barrier.texture_memory_barriers.push_back(tb1);
                compute->InsertBarrier(barrier);
            }

            auto ao_blur_ds = ssao->ssao_blur_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);

            {
                ao_blur_ds->SetResource(ssao->ssao_factor_texture, "ssao_blur_in");
                ao_blur_ds->SetResource(ssao->ssao_blur_texture, "ssao_blur_out");

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
                tb1.texture = ssao->ssao_blur_texture;
                barrier.texture_memory_barriers.push_back(tb1);
                compute->InsertBarrier(barrier);
            }
            //auto light_culling_ds = geometry->light_culling_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);
            ////light_culling_ds->SetResource();
            //// light culling
            //{
            //    compute->BindPipeline(geometry->light_culling_pass);
            //    compute->BindDescriptorSets(geometry->light_culling_pass, light_culling_ds);
            //    compute->Dispatch(geometry->slices[0], geometry->slices[1], geometry->slices[2]);
            //}
            auto shading_ds = shading->shading_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);
            // shading pass
            {

                shading_ds->SetResource(geometry->gbuffer0->GetTexture(), "gbuffer0_tex");
                shading_ds->SetResource(geometry->gbuffer1->GetTexture(), "gbuffer1_tex");
                shading_ds->SetResource(geometry->gbuffer2->GetTexture(), "gbuffer2_tex");
                shading_ds->SetResource(geometry->gbuffer3->GetTexture(), "gbuffer3_tex");
                shading_ds->SetResource(geometry->depth->GetTexture(), "depth_tex");
                shading_ds->SetResource(sampler, "default_sampler");
                shading_ds->SetResource(scene->m_scene_manager->scene_constants_buffer, "SceneConstants");
                shading_ds->SetResource(scene->m_scene_manager->GetLightCountBuffer(), "LightCountUb");
                shading_ds->SetResource(scene->m_scene_manager->GetLightParamBuffer(), "LightDataUb");
                shading_ds->SetResource(shading->shading_color_texture, "out_color");
                shading_ds->SetResource(ssao->ssao_blur_texture, "ao_tex");
                shading_ds->SetResource(shading->diffuse_irradiance_sh3_buffer, "DiffuseIrradianceSH3");
                shading_ds->SetResource(shading->prefiltered_irradiance_env_map, "specular_map");
                shading_ds->SetResource(shading->brdf_lut, "specular_brdf_lut");
                shading_ds->SetResource(shading->ibl_sampler, "ibl_sampler");
                shading_ds->Update();

                compute->BindPipeline(shading->shading_pass);
                compute->BindDescriptorSets(shading->shading_pass, shading_ds);
                compute->Dispatch(_width / 8 + 1, _height / 8 + 1, 1);
            }

            {
                BarrierDesc color_image_barrier{};

                TextureBarrierDesc tb;
                tb.src_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                tb.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                tb.texture = shading->shading_color_texture;
                color_image_barrier.texture_memory_barriers.push_back(tb);

                compute->InsertBarrier(color_image_barrier);
            }

            auto pp_ds = post_process->post_process_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);
            {
                pp_ds->SetResource(shading->shading_color_texture, "color_image");
                pp_ds->SetResource(post_process->pp_color_texture, "out_color_image");
                pp_ds->SetResource(post_process->exposure_constants_buffer, "exposure_constants");

                pp_ds->Update();

                compute->BindPipeline(post_process->post_process_pass);
                compute->BindDescriptorSets(post_process->post_process_pass, pp_ds);
                compute->Dispatch(_width / 8 + 1, _height / 8 + 1, 1);
            }

            {
                {
                    BarrierDesc pp_image_barrier{};
                    {
                        TextureBarrierDesc tb;
                        tb.src_state = ResourceState::RESOURCE_STATE_UNDEFINED;
                        tb.dst_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                        tb.texture = swap_chain->GetRenderTarget()->GetTexture();
                        pp_image_barrier.texture_memory_barriers.push_back(tb);
                    }

                    compute->InsertBarrier(pp_image_barrier);
                }

                auto taa_ds = antialiasing->taa_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);
                {
                    taa_ds->SetResource(antialiasing->previous_color_texture, "prev_color_tex");
                    taa_ds->SetResource(post_process->pp_color_texture, "curr_color_tex");
                    taa_ds->SetResource(geometry->gbuffer4->GetTexture(), "mv_tex");
                    taa_ds->SetResource(antialiasing->output_color_texture, "out_color_tex");
                    taa_ds->SetResource(scene->m_scene_manager->scene_constants_buffer, "SceneConstants");

                    taa_ds->Update();

                    compute->BindPipeline(antialiasing->taa_pass);
                    compute->BindDescriptorSets(antialiasing->taa_pass, taa_ds);
                    compute->Dispatch(_width / 8 + 1, _height / 8 + 1, 1);
                }
                {
                    BarrierDesc barrier{};

                    {
                        TextureBarrierDesc tb;
                        tb.src_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                        tb.dst_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
                        tb.texture = antialiasing->output_color_texture;
                        barrier.texture_memory_barriers.push_back(tb);
                    }

                    compute->InsertBarrier(barrier);
                }

                compute->CopyTexture(antialiasing->output_color_texture, swap_chain->GetRenderTarget()->GetTexture());

                {
                    BarrierDesc barrier{};

                    TextureBarrierDesc tb;
                    tb.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                    tb.dst_state = ResourceState::RESOURCE_STATE_PRESENT;
                    tb.texture = swap_chain->GetRenderTarget()->GetTexture();
                    barrier.texture_memory_barriers.push_back(tb);

                     tb.src_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                     tb.dst_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                     tb.texture = antialiasing->previous_color_texture;
                     barrier.texture_memory_barriers.push_back(tb);

                     //tb.src_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                     //tb.dst_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
                     //tb.texture = post_process->pp_color_image;
                     //barrier.texture_memory_barriers.push_back(tb);
                    compute->InsertBarrier(barrier);

                }

                compute->CopyTexture(antialiasing->output_color_texture, antialiasing->previous_color_texture);
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
