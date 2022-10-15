#include "pbs.h"

void Pbr::InitAPI() {
    rhi = engine->m_render_system->GetRhi();

    m_camera = std::make_unique<Camera>(Math::float3(0.0, 0.0, 1.0_m), Math::float3(0.0, 0.0, 0.0),
                                        Math::float3(0.0, 1.0_m, 0.0));
    m_camera->SetCameraSpeed(0.1);
    m_camera->SetExposure(16.0f, 1 / 125.0f, 100.0f);
    engine->m_render_system->SetCamera(m_camera.get());
    engine->m_input_system->SetCamera(engine->m_render_system->GetDebugCamera());

    swap_chain = rhi->CreateSwapChain(SwapChainCreateInfo{2});
}

void Pbr::InitResources() {

    // shaders
    {
        std::filesystem::path geometry_vs_path = asset_path / "shaders/gbuffer.vert.hsl";

        std::filesystem::path geometry_ps_path = asset_path / "shaders/gbuffer.frag.hsl";

        std::filesystem::path shading_cs_path = asset_path / "shaders/deferred_shading.comp.hsl";

        std::filesystem::path postprocess_cs_path = asset_path / "shaders/post_process.comp.hsl";

        geometry_vs = rhi->CreateShader(ShaderType::VERTEX_SHADER, 0, geometry_vs_path);

        geometry_ps = rhi->CreateShader(ShaderType::PIXEL_SHADER, 0, geometry_ps_path);

        shading_cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, 0, shading_cs_path);

        post_process_cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, 0, postprocess_cs_path);
    }

    // graphics pass
    {
        gbuffer0 = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_SNORM,
                                                             RenderTargetType::COLOR, width, height});
        gbuffer1 = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM,
                                                                  RenderTargetType::COLOR, width, height});
        gbuffer2 = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM,
                                                                  RenderTargetType::COLOR, width, height});
        gbuffer3 = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM,
                                                                  RenderTargetType::COLOR, width, height});

        depth = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_D32_SFLOAT,
                                                               RenderTargetType::DEPTH_STENCIL, width, height});

        graphics_pass_ci.vertex_input_state.attribute_count = 4;

        auto &pos = graphics_pass_ci.vertex_input_state.attributes[0];
        pos.attrib_format = VertexAttribFormat::F32; // position
        pos.portion = 3;
        pos.binding = 0;
        pos.location = 0;
        pos.offset = 0;
        pos.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;

        auto &normal = graphics_pass_ci.vertex_input_state.attributes[1];
        normal.attrib_format = VertexAttribFormat::F32; // normal, TOOD: SN16 is a better format
        normal.portion = 3;
        normal.binding = 0;
        normal.location = 1;
        normal.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        normal.offset = offsetof(Vertex, normal);

        auto &uv0 = graphics_pass_ci.vertex_input_state.attributes[2];
        uv0.attrib_format = VertexAttribFormat::F32; // uv0 TOOD: UN16 is a better format
        uv0.portion = 2;
        uv0.binding = 0;
        uv0.location = 2;
        uv0.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        uv0.offset = offsetof(Vertex, uv0);

        auto &uv1 = graphics_pass_ci.vertex_input_state.attributes[3];
        uv1.attrib_format = VertexAttribFormat::F32; // uv1 TOOD: UN16 is a better format
        uv1.portion = 2;
        uv1.binding = 0;
        uv1.location = 3;
        uv1.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        uv1.offset = offsetof(Vertex, uv1);

        graphics_pass_ci.view_port_state.width = width;
        graphics_pass_ci.view_port_state.height = height;

        graphics_pass_ci.depth_stencil_state.depth_func = DepthFunc::LESS;
        graphics_pass_ci.depth_stencil_state.depthNear = 0.0f;
        graphics_pass_ci.depth_stencil_state.depthNear = 1.0f;
        graphics_pass_ci.depth_stencil_state.depth_test = true;
        graphics_pass_ci.depth_stencil_state.depth_write = true;
        graphics_pass_ci.depth_stencil_state.stencil_enabled = false;

        graphics_pass_ci.input_assembly_state.topology = PrimitiveTopology::TRIANGLE_LIST;

        graphics_pass_ci.multi_sample_state.sample_count = 1;

        graphics_pass_ci.rasterization_state.cull_mode = CullMode::BACK;
        graphics_pass_ci.rasterization_state.discard = false;
        graphics_pass_ci.rasterization_state.fill_mode = FillMode::TRIANGLE;
        graphics_pass_ci.rasterization_state.front_face = FrontFace::CCW;

        graphics_pass_ci.render_target_formats.color_attachment_count = 4;
        graphics_pass_ci.render_target_formats.color_attachment_formats = std::vector<TextureFormat>{gbuffer0->GetTexture()->m_format, gbuffer1->GetTexture()->m_format,
                                       gbuffer2->GetTexture()->m_format, gbuffer3->GetTexture()->m_format};
        graphics_pass_ci.render_target_formats.has_depth = true;
        graphics_pass_ci.render_target_formats.depth_stencil_format = depth->GetTexture()->m_format;

        geometry_pass = rhi->CreateGraphicsPipeline(graphics_pass_ci);

        shading_pass = rhi->CreateComputePipeline(ComputePipelineCreateInfo{});

        post_process_pass = rhi->CreateComputePipeline(ComputePipelineCreateInfo{});

        shading_color_image = rhi->CreateTexture(TextureCreateInfo{
            DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
            TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA16_SFLOAT, width, height, 1, false});

        pp_color_image = rhi->CreateTexture(TextureCreateInfo{
            DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
            TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, width, height, 1, false});
    }

    // resources{

    {
        auto mesh1 =
            new Mesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL | VertexAttributeType::UV0});
        // mesh->LoadMesh(asset_path / "models/DamagedHelmet/DamagedHelmet.gltf");
        mesh1->LoadMesh(asset_path / "models/rhulk/Rhulk, Disciple of the Witness.gltf");

        mesh1->CreateGpuResources(rhi);

        auto mesh2 =
            new Mesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL | VertexAttributeType::UV0});
        //mesh2->LoadMesh(asset_path / "models/Sponza/glTF/Sponza.gltf");
        mesh2->LoadMesh(asset_path / "models/riven/Riven (rigged by BreamVr).gltf");
        mesh2->CreateGpuResources(rhi);

        meshes.push_back(mesh1);
        meshes.push_back(mesh2);
    }

    SamplerDesc sampler_desc{};
    sampler_desc.min_filter = FilterType::FILTER_LINEAR;
    sampler_desc.mag_filter = FilterType::FILTER_LINEAR;
    sampler_desc.mip_map_mode = MipMapMode::MIPMAP_MODE_LINEAR;
    sampler_desc.address_u = AddressMode::ADDRESS_MODE_REPEAT;
    sampler_desc.address_v = AddressMode::ADDRESS_MODE_REPEAT;
    sampler_desc.address_w = AddressMode::ADDRESS_MODE_REPEAT;

    sampler = rhi->GetSampler(sampler_desc);

    // camera
    {
        cam = engine->m_render_system->GetDebugCamera();

        cam->SetPerspectiveProjectionMatrix(90.0_deg, (float)width / (float)height, 0.1f, 100.0f);

        camera_buffer =
            rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                               ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(CameraUb)});
    }

    // light
    {
        std::random_device seed;
        std::ranlux48 engine(seed());
        std::uniform_real_distribution<f32> random_position(-10.0, 10.0);
        std::uniform_real_distribution<f32> random_color(0.5, 1.0);

        for (unsigned int i = 0; i < 64; i++) {
            // calculate slightly random offsets
            float xPos = random_position(engine);
            float yPos = random_position(engine);
            float zPos = random_position(engine) + 2.0f;

            // also calculate random color
            float rColor = random_color(engine);
            float gColor = random_color(engine);
            float bColor = random_color(engine);

            Math::float3 pos(xPos, yPos, zPos);
            Math::float3 col(rColor, gColor, bColor);

            //lights.push_back(new PointLight(col, 1000000.0_lm, pos, 10.0));
        }

        lights.push_back(new DirectionalLight(Math::float3(1.0, 1.0, 1.0), 120000.0_lux, Math::float3(0.0, 0.0, -1.0)));
        for (auto &l : lights) {
            lights_param_buffer.push_back(l->GetParamBuffer());
        }
        light_count = lights.size();
        light_count_buffer =
            rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                               ResourceState::RESOURCE_STATE_SHADER_RESOURCE, 4 * sizeof(u32)});

        light_buffer = rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                                          ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                                                          sizeof(LightParams) * light_count});
        deferred_shading_constants_buffer = rhi->CreateBuffer(
            BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                             ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(DeferredShadingConstants)});
    }
}

void Pbr::run() {

    auto resource_uploaded_semaphore = rhi->GetSemaphore();
    bool first_frame = false;
    bool &resources_uploaded = first_frame;

    geometry_pass->SetGraphicsShader(geometry_vs, geometry_ps);
    shading_pass->SetComputeShader(shading_cs);
    post_process_pass->SetComputeShader(post_process_cs);

    for (;;) {

        engine->m_input_system->Tick();

        rhi->AcquireNextFrame(swap_chain.get());

        camera_ub.vp = cam->GetViewProjectionMatrix();
        camera_ub.camera_pos = cam->GetPosition();
        camera_ub.exposure = cam->GetExposure();

        deferred_shading_constants.camera_pos = Math::float4(cam->GetPosition());
        deferred_shading_constants.inverse_vp = cam->GetInvViewProjectionMatrix();
        deferred_shading_constants.width = width;
        deferred_shading_constants.height= height;

        auto geometry_pass_per_frame_ds = geometry_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);

        // resource upload
        {
            auto transfer = rhi->GetCommandList(CommandQueueType::TRANSFER);

            transfer->BeginRecording();
            transfer->UpdateBuffer(light_buffer.get(), lights_param_buffer.data(),
                                   lights_param_buffer.size() * sizeof(LightParams));
            transfer->UpdateBuffer(light_count_buffer.get(), &light_count, sizeof(light_count) * 4);
            transfer->UpdateBuffer(camera_buffer.get(), &camera_ub, sizeof(CameraUb));
            transfer->UpdateBuffer(deferred_shading_constants_buffer.get(), &deferred_shading_constants, sizeof(DeferredShadingConstants));
            if (!resources_uploaded) {
                resources_uploaded = true;
                for (auto &mesh : meshes) {
                    mesh->UploadResources(transfer);
                }
            }

            transfer->EndRecording();

            {
                QueueSubmitInfo submit_info{};
                submit_info.queue_type = CommandQueueType::TRANSFER;
                submit_info.command_lists.push_back(transfer);
                if (!resources_uploaded)
                    submit_info.signal_semaphores.push_back(resource_uploaded_semaphore.get());
                rhi->SubmitCommandLists(submit_info);
            }
        }


        // perframe descriptor set
        geometry_pass_per_frame_ds->SetResource(camera_buffer.get(), "CameraParamsUb");
        geometry_pass_per_frame_ds->Update();

        // material descriptor set
        for (auto &mesh : meshes) {
            for (auto &material : mesh->GetMaterials()) {
                if (material.GetShadingModelID() == ShadingModel::SHADING_MODEL_OPAQUE) {
                    material.material_descriptor_set =
                        geometry_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_BATCH);
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

        {
            auto cl = rhi->GetCommandList(CommandQueueType::GRAPHICS);
            cl->BeginRecording();

            {
                BarrierDesc image_barriers{};

                TextureBarrierDesc tb;
                tb.src_state = ResourceState::RESOURCE_STATE_UNDEFINED;
                tb.dst_state = ResourceState::RESOURCE_STATE_RENDER_TARGET;
                tb.texture = gbuffer0->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                tb.texture = gbuffer1->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                tb.texture = gbuffer2->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                tb.texture = gbuffer3->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                tb.dst_state = ResourceState::RESOURCE_STATE_DEPTH_WRITE;
                tb.texture = depth->GetTexture();
                image_barriers.texture_memory_barriers.push_back(tb);
                cl->InsertBarrier(image_barriers);
            }

            RenderPassBeginInfo begin_info{};
            begin_info.render_area = Rect{0, 0, width, height};
            begin_info.render_targets[0].data = gbuffer0.get();
            begin_info.render_targets[0].clear_color = ClearValueColor{Math::float4(0.0)};
            begin_info.render_targets[1].data = gbuffer1.get();
            begin_info.render_targets[1].clear_color = ClearValueColor{Math::float4(0.0)};
            begin_info.render_targets[2].data = gbuffer2.get();
            begin_info.render_targets[2].clear_color = ClearValueColor{Math::float4(0.0)};
            begin_info.render_targets[3].data = gbuffer3.get();
            begin_info.render_targets[3].clear_color = ClearValueColor{Math::float4(0.0)};
            begin_info.depth_stencil.data = depth.get();
            begin_info.depth_stencil.clear_color = ClearValueDepthStencil{1.0, 0};

            cl->BindDescriptorSets(geometry_pass, geometry_pass_per_frame_ds);

            cl->BeginRenderPass(begin_info);

            cl->BindPipeline(geometry_pass);
            for (auto &mesh : meshes) {
                u32 offset = 0;
                auto vb = mesh->GetVertexBuffer();
                cl->BindVertexBuffers(1, &vb, &offset);
                cl->BindIndexBuffer(mesh->GetIndexBuffer(), 0);

                for (auto &node : mesh->GetNodes()) {
                    if (node.mesh_primitives.empty()) {
                        continue;
                    }
                    auto mat = node.GetModelMatrix().Transpose();
                    cl->BindPushConstant(geometry_pass, "root_constant_model_mat", &mat);

                    for (auto &m : node.mesh_primitives) {
                        auto &material = mesh->GetMaterial(m->material_id);
                        if (material.GetShadingModelID() == ShadingModel::SHADING_MODEL_OPAQUE) {

                            cl->BindDescriptorSets(geometry_pass, material.material_descriptor_set);
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
                tb.texture = gbuffer0->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);
                tb.texture = gbuffer1->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);
                tb.texture = gbuffer2->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);
                tb.texture = gbuffer3->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);
                tb.src_state = ResourceState::RESOURCE_STATE_DEPTH_WRITE;
                tb.texture = depth->GetTexture();
                barrier.texture_memory_barriers.push_back(tb);
                
                TextureBarrierDesc tb1;
                tb1.src_state = ResourceState::RESOURCE_STATE_UNDEFINED;
                tb1.dst_state = ResourceState::RESOURCE_STATE_UNORDERED_ACCESS;
                tb1.texture = shading_color_image.get();
                barrier.texture_memory_barriers.push_back(tb1);
                tb1.texture = pp_color_image.get();
                barrier.texture_memory_barriers.push_back(tb1);

                cl->InsertBarrier(barrier);

            }

            cl->EndRecording();

            // auto gp_semaphore = rhi->GetSemaphore();
            {
                QueueSubmitInfo submit_info{};
                submit_info.queue_type = CommandQueueType::GRAPHICS;
                submit_info.command_lists.push_back(cl);
                // submit_info.wait_semaphores.push_back(resource_uploaded_semaphore.get());
                // submit_info.signal_semaphores.push_back(gp_semaphore.get());
                submit_info.wait_image_acquired = true;
                rhi->SubmitCommandLists(submit_info);
            }
        }


        auto shading_ds = shading_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);
        // shading pass
        //auto shading_sm = rhi->GetSemaphore();
        {

            shading_ds->SetResource(gbuffer0->GetTexture(), "gbuffer0_tex");
            shading_ds->SetResource(gbuffer1->GetTexture(), "gbuffer1_tex");
            shading_ds->SetResource(gbuffer2->GetTexture(), "gbuffer2_tex");
            shading_ds->SetResource(gbuffer3->GetTexture(), "gbuffer3_tex");
            shading_ds->SetResource(depth->GetTexture(), "depth_tex");
            shading_ds->SetResource(sampler.get(), "default_sampler");
            shading_ds->SetResource(deferred_shading_constants_buffer.get(), "DeferredShadingConstants");
            shading_ds->SetResource(light_count_buffer.get(), "LightCountUb");
            shading_ds->SetResource(light_buffer.get(), "LightDataUb");
            shading_ds->SetResource(shading_color_image.get(), "out_color");
            shading_ds->Update();

            auto compute = rhi->GetCommandList(CommandQueueType::COMPUTE);
            compute->BeginRecording();


            compute->BindPipeline(shading_pass);
            compute->BindDescriptorSets(shading_pass, shading_ds);
            compute->Dispatch(width / 8 + 1, height / 8 + 1, 1);
            compute->EndRecording();

            {
                QueueSubmitInfo submit_info{};
                submit_info.queue_type = CommandQueueType::COMPUTE;
                submit_info.command_lists.push_back(compute);
                // submit_info.wait_semaphores.push_back(gp_semaphore.get());
                //submit_info.signal_semaphores.push_back(shading_sm.get());
                rhi->SubmitCommandLists(submit_info);
            }
        }

        auto pp_ds = post_process_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);
        {
            pp_ds->SetResource(shading_color_image.get(), "color_image");
            pp_ds->SetResource(pp_color_image.get(), "out_color_image");
            pp_ds->SetResource(camera_buffer.get(), "CameraParamsUb");

            pp_ds->Update();

            auto compute = rhi->GetCommandList(CommandQueueType::COMPUTE);
            compute->BeginRecording();

            compute->BindPipeline(post_process_pass);
            compute->BindDescriptorSets(post_process_pass, pp_ds);
            compute->Dispatch(width / 8 + 1, height / 8 + 1, 1);
            compute->EndRecording();

            {
                QueueSubmitInfo submit_info{};
                submit_info.queue_type = CommandQueueType::COMPUTE;
                submit_info.command_lists.push_back(compute);
                //submit_info.wait_semaphores.push_back(shading_sm.get());
                rhi->SubmitCommandLists(submit_info);
            }
        }

        {
            auto transfer = rhi->GetCommandList(CommandQueueType::TRANSFER);
            transfer->BeginRecording();
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
                tb2.texture = pp_color_image.get();
                pp_image_barrier.texture_memory_barriers.push_back(tb2);

                transfer->InsertBarrier(pp_image_barrier);
            }
            transfer->CopyTexture(pp_color_image.get(), swap_chain->GetRenderTarget()->GetTexture());
            {
                BarrierDesc swap_chain_image_barrier{};

                TextureBarrierDesc tb;
                tb.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
                tb.dst_state = ResourceState::RESOURCE_STATE_PRESENT;
                tb.texture = swap_chain->GetRenderTarget()->GetTexture();
                swap_chain_image_barrier.texture_memory_barriers.push_back(tb);

                transfer->InsertBarrier(swap_chain_image_barrier);
            }
            transfer->EndRecording();
            {
                QueueSubmitInfo submit_info{};
                submit_info.queue_type = CommandQueueType::TRANSFER;
                submit_info.command_lists.push_back(transfer);
                // submit_info.wait_semaphores.push_back(gp_semaphore.get());
                // submit_info.signal_semaphores.push_back(pp_semaphore.get());
                submit_info.signal_render_complete = true;
                rhi->SubmitCommandLists(submit_info);
            }
        }
        // rhi->WaitGpuExecution(CommandQueueType::TRANSFER);
        {
            QueuePresentInfo opaque_pass_ci{};
            opaque_pass_ci.swap_chain = swap_chain.get();
            rhi->Present(opaque_pass_ci);
        }
        // Horizon::RDC::EndFrameCapture();
        // LOG_DEBUG("total mesh: {} culled mesh: {}", total_mesh, culled_mesh);
    }
    // rhi->DestroyPipeline(graphics_pass);
    //// rhi->DestroyPipeline(post_process_pass);
    // rhi->DestroyShader(vs);
    // rhi->DestroyShader(ps);
    // rhi->DestroyShader(cs);
    LOG_INFO("draw done");
}
