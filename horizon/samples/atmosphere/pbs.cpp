#include "pbs.h"

void Pbr::InitAPI() {
    rhi = engine->m_render_system->GetRhi();

    m_camera =
        std::make_unique<Camera>(Math::float3(0.0, 0.0, 1.0), Math::float3(0.0, 0.0, 0.0), Math::float3(0.0, 1.0, 0.0));
    m_camera->SetCameraSpeed(0.1);
    m_camera->SetExposure(16.0f, 1 / 125.0f, 100.0f);
    engine->m_render_system->SetCamera(m_camera.get());
    engine->m_input_system->SetCamera(engine->m_render_system->GetDebugCamera());

    swap_chain = rhi->CreateSwapChain(SwapChainCreateInfo{2});
}

void Pbr::InitResources() {

    opaque_vs_path = asset_path / "shaders/lit_opaque.vert.hsl";
    opaque_ps_path = asset_path / "shaders/lit_opaque.frag.hsl";

    masked_vs_path = asset_path / "shaders/lit_masked.vert.hsl";
    masked_ps_path = asset_path / "shaders/lit_masked.frag.hsl";

    opaque_vs = rhi->CreateShader(ShaderType::VERTEX_SHADER, 0, opaque_vs_path);

    opaque_ps = rhi->CreateShader(ShaderType::PIXEL_SHADER, 0, opaque_ps_path);

        masked_vs = rhi->CreateShader(ShaderType::VERTEX_SHADER, 0, masked_vs_path);

    masked_ps = rhi->CreateShader(ShaderType::PIXEL_SHADER, 0, masked_ps_path);

    // cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, 0, cs_path);

    // auto rt0 = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM,
    //                                                           RenderTargetType::COLOR, width, height});
    rt0 = swap_chain->GetRenderTarget();
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

    graphics_pass_ci.render_target_formats.color_attachment_count = 1;
    graphics_pass_ci.render_target_formats.color_attachment_formats = std::vector<TextureFormat>{rt0->GetTexture()->m_format};
    graphics_pass_ci.render_target_formats.has_depth = true;
    graphics_pass_ci.render_target_formats.depth_stencil_format = depth->GetTexture()->m_format;

    opaque_pass = rhi->CreateGraphicsPipeline(graphics_pass_ci);

    graphics_pass_ci.rasterization_state.discard = true;

    masked_pass = rhi->CreateGraphicsPipeline(graphics_pass_ci);

    mesh = new Mesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL | VertexAttributeType::UV0});
    mesh->LoadMesh(asset_path /"models/DamagedHelmet/DamagedHelmet.gltf");
    // mesh->LoadMesh(asset_path /"models/FlightHelmet/glTF/FlightHelmet.gltf");
    //mesh->LoadMesh(asset_path / "models/Sponza/glTF/Sponza.gltf");
    mesh->CreateGpuResources(rhi);
    // InitSphere();
    SamplerDesc sampler_desc{};
    sampler_desc.min_filter = FilterType::FILTER_LINEAR;
    sampler_desc.mag_filter = FilterType::FILTER_LINEAR;
    sampler_desc.mip_map_mode = MipMapMode::MIPMAP_MODE_LINEAR;
    sampler_desc.address_u = AddressMode::ADDRESS_MODE_REPEAT;
    sampler_desc.address_v = AddressMode::ADDRESS_MODE_REPEAT;
    sampler_desc.address_w = AddressMode::ADDRESS_MODE_REPEAT;

    sampler = rhi->GetSampler(sampler_desc);

    cam = engine->m_render_system->GetDebugCamera();

    cam->SetPerspectiveProjectionMatrix(90.0f, (float)width / (float)height, 0.1f, 100.0f);

    camera_buffer =
        rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                           ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(CameraUb)});

    light_count = 1;

    light_count_buffer =
        rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                           ResourceState::RESOURCE_STATE_SHADER_RESOURCE, 4 * sizeof(u32)});

    light_buffer =
        rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                           ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(LightParams)});

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

        //lights.push_back(new PointLight(col, 1000000.0, pos, 10.0));
    }

    lights.push_back(new DirectionalLight(Math::float3(1.0, 1.0, 1.0), 120000.0, Math::float3(0.0, 0.0, -1.0)));
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
}

void Pbr::run() {

    auto resource_uploaded_semaphore = rhi->GetSemaphore();
    bool resources_uploaded = false;

    for (;;) {
        culled_mesh = 0;
        total_mesh = 0;
        // Horizon::RDC::StartFrameCapture();
        engine->m_input_system->Tick();

        rhi->AcquireNextFrame(swap_chain.get());
        camera_ub.vp = cam->GetViewProjectionMatrix();
        camera_ub.camera_pos = cam->GetPosition();
        camera_ub.exposure = cam->GetExposure();
        opaque_pass->SetGraphicsShader(opaque_vs, opaque_ps);
        masked_pass->SetGraphicsShader(masked_vs, masked_ps);


        auto opaque_pass_per_frame_ds = opaque_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);

        auto transfer = rhi->GetCommandList(CommandQueueType::TRANSFER);

        transfer->BeginRecording();
        transfer->UpdateBuffer(light_buffer.get(), lights_param_buffer.data(),
                               lights_param_buffer.size() * sizeof(LightParams));
        transfer->UpdateBuffer(light_count_buffer.get(), &light_count, sizeof(light_count) * 4);
        transfer->UpdateBuffer(camera_buffer.get(), &camera_ub, sizeof(CameraUb));

        if (!resources_uploaded) {
            resources_uploaded = true;
            mesh->UploadResources(transfer);
        }

        transfer->EndRecording();

        {
            QueueSubmitInfo submit_info{};
            submit_info.queue_type = CommandQueueType::TRANSFER;
            submit_info.command_lists.push_back(transfer);
            submit_info.signal_semaphores.push_back(resource_uploaded_semaphore.get());
            rhi->SubmitCommandLists(submit_info);
        }

        // perframe descriptor set
        opaque_pass_per_frame_ds->SetResource(camera_buffer.get(), 0);
        opaque_pass_per_frame_ds->SetResource(light_buffer.get(), 2);
        opaque_pass_per_frame_ds->SetResource(light_count_buffer.get(), 1);
        opaque_pass_per_frame_ds->Update();

        // material descriptor set
        for (auto &node : mesh->GetNodes()) {
            if (node.mesh_primitives.empty()) {
                continue;
            }
            std::unordered_set<Material *> materials{};
            for (const auto &m : node.mesh_primitives) {
                materials.emplace(&mesh->GetMaterial(m->material_id));
            }

            // opaque material
            for (auto &material : materials) {
                if (material->GetShadingModelID() == ShadingModel::SHADING_MODEL_OPAQUE) {

                    material->material_descriptor_set =
                        opaque_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_BATCH);

                    material->material_descriptor_set->SetResource(
                        material->material_textures.at(MaterialTextureType::BASE_COLOR).texture.get(), 0);
                    material->material_descriptor_set->SetResource(
                        material->material_textures.at(MaterialTextureType::NORMAL).texture.get(), 1);
                    material->material_descriptor_set->SetResource(
                        material->material_textures.at(MaterialTextureType::METALLIC_ROUGHTNESS).texture.get(), 2);
                    material->material_descriptor_set->SetResource(
                        material->material_textures.at(MaterialTextureType::EMISSIVE).texture.get(), 3);

                    material->material_descriptor_set->SetResource(sampler.get(), 4);
                    material->material_descriptor_set->SetResource(material->param_buffer.get(), 5);
                    material->material_descriptor_set->Update();
                } else if (material->GetShadingModelID() == ShadingModel::SHADING_MODEL_MASKED) {

                    material->material_descriptor_set =
                        masked_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_BATCH);

                    material->material_descriptor_set->SetResource(
                        material->material_textures.at(MaterialTextureType::BASE_COLOR).texture.get(), 0);
                    material->material_descriptor_set->SetResource(
                        material->material_textures.at(MaterialTextureType::NORMAL).texture.get(), 1);
                    material->material_descriptor_set->SetResource(
                        material->material_textures.at(MaterialTextureType::METALLIC_ROUGHTNESS).texture.get(), 2);
                    material->material_descriptor_set->SetResource(
                        material->material_textures.at(MaterialTextureType::EMISSIVE).texture.get(), 3);
                    material->material_descriptor_set->SetResource(
                        material->material_textures.at(MaterialTextureType::ALPHA_MASK).texture.get(), 4);

                    material->material_descriptor_set->SetResource(sampler.get(), 5);
                    material->material_descriptor_set->SetResource(material->param_buffer.get(), 6);
                    material->material_descriptor_set->Update();
                }
            }
        }

        auto cl = rhi->GetCommandList(CommandQueueType::GRAPHICS);
        cl->BeginRecording();

        {
            BarrierDesc swap_chain_image_barrier{};

            TextureBarrierDesc tb;
            tb.src_state = ResourceState::RESOURCE_STATE_UNDEFINED;
            tb.dst_state = ResourceState::RESOURCE_STATE_RENDER_TARGET;
            tb.texture = swap_chain->GetRenderTarget()->GetTexture();
            swap_chain_image_barrier.texture_memory_barriers.push_back(tb);

            cl->InsertBarrier(swap_chain_image_barrier);
        }

        RenderPassBeginInfo begin_info{};
        begin_info.render_area = Rect{0, 0, width, height};
        begin_info.render_targets[0].data = swap_chain->GetRenderTarget();
        begin_info.render_targets[0].clear_color = ClearValueColor{Math::float4(0.0)};
        begin_info.depth_stencil.data = depth.get();
        begin_info.depth_stencil.clear_color = ClearValueDepthStencil{1.0, 0};

        u32 offset = 0;
        auto vb = mesh->GetVertexBuffer();
        cl->BindVertexBuffers(1, &vb, &offset);
        cl->BindIndexBuffer(mesh->GetIndexBuffer(), 0);

        cl->BeginRenderPass(begin_info);
        cl->BindPipeline(opaque_pass);

        cl->BindDescriptorSets(opaque_pass, opaque_pass_per_frame_ds.get());

        for (auto &node : mesh->GetNodes()) {
            if (node.mesh_primitives.empty()) {
                continue;
            }
            auto mat = node.GetModelMatrix().Transpose();
            cl->BindPushConstant(opaque_pass, "model_matrix", &mat);

            for (auto &m : node.mesh_primitives) {
                auto &material = mesh->GetMaterial(m->material_id);
                if (material.GetShadingModelID() == ShadingModel::SHADING_MODEL_OPAQUE) {

                    cl->BindDescriptorSets(opaque_pass, material.material_descriptor_set.get());
                    cl->DrawIndexedInstanced(m->index_count, m->index_offset, 0);
                }
            }
        }

        cl->BindPipeline(masked_pass);

        cl->BindDescriptorSets(opaque_pass, opaque_pass_per_frame_ds.get());

        for (auto &node : mesh->GetNodes()) {
            if (node.mesh_primitives.empty()) {
                continue;
            }
            auto mat = node.GetModelMatrix().Transpose();
            cl->BindPushConstant(masked_pass, "model_matrix", &mat);

            for (auto &m : node.mesh_primitives) {
                auto &material = mesh->GetMaterial(m->material_id);
                if (material.GetShadingModelID() == ShadingModel::SHADING_MODEL_MASKED) {

                    cl->BindDescriptorSets(masked_pass, material.material_descriptor_set.get());
                    cl->DrawIndexedInstanced(m->index_count, m->index_offset, 0);
                }
            }
        }

        cl->EndRenderPass();

        {
            BarrierDesc swap_chain_image_barrier{};

            TextureBarrierDesc tb;
            tb.src_state = ResourceState::RESOURCE_STATE_RENDER_TARGET;
            tb.dst_state = ResourceState::RESOURCE_STATE_PRESENT;
            tb.texture = swap_chain->GetRenderTarget()->GetTexture();
            swap_chain_image_barrier.texture_memory_barriers.push_back(tb);

            cl->InsertBarrier(swap_chain_image_barrier);
        }

        cl->EndRecording();

        auto gp_semaphore = rhi->GetSemaphore();
        {
            QueueSubmitInfo submit_info{};
            submit_info.queue_type = CommandQueueType::GRAPHICS;
            submit_info.command_lists.push_back(cl);
            submit_info.wait_semaphores.push_back(resource_uploaded_semaphore.get());
            submit_info.wait_image_acquired = true;
            submit_info.signal_render_complete = true;
            rhi->SubmitCommandLists(submit_info);
        }

        {
            QueuePresentInfo opaque_pass_ci{};
            opaque_pass_ci.swap_chain = swap_chain.get();
            rhi->Present(opaque_pass_ci);
        }
        // Horizon::RDC::EndFrameCapture();
        //LOG_DEBUG("total mesh: {} culled mesh: {}", total_mesh, culled_mesh);
    }
    //rhi->DestroyPipeline(graphics_pass);
    //// rhi->DestroyPipeline(post_process_pass);
    //rhi->DestroyShader(vs);
    //rhi->DestroyShader(ps);
    //rhi->DestroyShader(cs);
    LOG_INFO("draw done");
}