#include "pbs.h"

void Pbr::InitAPI() {    
    rhi = engine->m_render_system->GetRhi();

    m_camera = std::make_unique<Camera>(Math::float3(0.0, 0.0, 1.0), Math::float3(0.0, 0.0, 0.0),
                                                         Math::float3(0.0, 1.0, 0.0));
    m_camera->SetCameraSpeed(0.1);
    m_camera->SetExposure(16.0f, 1 / 125.0f, 100.0f);
    engine->m_render_system->SetCamera(m_camera.get());
    engine->m_input_system->SetCamera(engine->m_render_system->GetDebugCamera());

    swap_chain = rhi->CreateSwapChain(SwapChainCreateInfo{2});
 
}

void Pbr::InitResources() {

    vs_path = asset_path / "shaders/pbr.vert.hsl";
    ps_path = asset_path / "shaders/pbr.frag.hsl";
    cs_path = asset_path / "shaders/post_process.comp.hsl";
    

    vs = rhi->CreateShader(ShaderType::VERTEX_SHADER, 0, vs_path);

    ps = rhi->CreateShader(ShaderType::PIXEL_SHADER, 0, ps_path);

    //cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, 0, cs_path);

    // auto rt0 = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM,
    //                                                           RenderTargetType::COLOR, width, height});
    rt0 = swap_chain->GetRenderTarget();
    depth = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_D32_SFLOAT,
                                                           RenderTargetType::DEPTH_STENCIL, width, height});

    info.vertex_input_state.attribute_count = 4;

    auto &pos = info.vertex_input_state.attributes[0];
    pos.attrib_format = VertexAttribFormat::F32; // position
    pos.portion = 3;
    pos.binding = 0;
    pos.location = 0;
    pos.offset = 0;
    pos.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;

    auto &normal = info.vertex_input_state.attributes[1];
    normal.attrib_format = VertexAttribFormat::F32; // normal, TOOD: SN16 is a better format
    normal.portion = 3;
    normal.binding = 0;
    normal.location = 1;
    normal.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
    normal.offset = offsetof(Vertex, normal);

    auto &uv0 = info.vertex_input_state.attributes[2];
    uv0.attrib_format = VertexAttribFormat::F32; // uv0 TOOD: UN16 is a better format
    uv0.portion = 2;
    uv0.binding = 0;
    uv0.location = 2;
    uv0.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
    uv0.offset = offsetof(Vertex, uv0);

    auto &uv1 = info.vertex_input_state.attributes[3];
    uv1.attrib_format = VertexAttribFormat::F32; // uv1 TOOD: UN16 is a better format
    uv1.portion = 2;
    uv1.binding = 0;
    uv1.location = 3;
    uv1.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
    uv1.offset = offsetof(Vertex, uv1);

    info.view_port_state.width = width;
    info.view_port_state.height = height;

    info.depth_stencil_state.depth_func = DepthFunc::LESS;
    info.depth_stencil_state.depthNear = 0.0f;
    info.depth_stencil_state.depthNear = 1.0f;
    info.depth_stencil_state.depth_test = true;
    info.depth_stencil_state.depth_write = true;
    info.depth_stencil_state.stencil_enabled = false;

    info.input_assembly_state.topology = PrimitiveTopology::TRIANGLE_LIST;

    info.multi_sample_state.sample_count = 1;

    info.rasterization_state.cull_mode = CullMode::BACK;
    info.rasterization_state.discard = false;
    info.rasterization_state.fill_mode = FillMode::TRIANGLE;
    info.rasterization_state.front_face = FrontFace::CCW;

    info.render_target_formats.color_attachment_count = 1;
    info.render_target_formats.color_attachment_formats = std::vector<TextureFormat>{rt0->GetTexture()->m_format};
    info.render_target_formats.has_depth = true;
    info.render_target_formats.depth_stencil_format = depth->GetTexture()->m_format;

    graphics_pass = rhi->CreateGraphicsPipeline(info);


    mesh = new Mesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL | VertexAttributeType::UV0});
    //mesh->LoadMesh(asset_path /"models/DamagedHelmet/DamagedHelmet.gltf");
    mesh->LoadMesh(asset_path /"models/FlightHelmet/glTF/FlightHelmet.gltf");
    //mesh->LoadMesh(asset_path / "models/Sponza/glTF/Sponza.gltf");
    mesh->CreateGpuResources(rhi);
    //InitSphere();
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
    directional_light = new DirectionalLight(Math::float3(1.0, 1.0, 1.0), 120000.0, Math::float3(0.0, 0.0, -1.0));

    light_buffer = rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                                      ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                                                      sizeof(LightParams)});
}

void Pbr::InitSphere() {
    int nrRows = 7;
    int nrColumns = 7;
    float spacing = 2.5;
    
    for (int i = 0; i < nrRows; i++) {

        for (int j = 0; j < nrColumns; j++) {
            Mesh *sphere = new Mesh(
                MeshDesc{{VertexAttributeType::POSTION | VertexAttributeType::NORMAL | VertexAttributeType::UV0}});
            sphere->LoadMesh(BasicGeometry::Shapes::SPHERE);
            sphere->GetNodes()[0].model_matrix;
            meshes.push_back(sphere);
            //sphere->CreateGpuResources(rhi);
        }
    }

    // render rows*column number of spheres with varying metallic/roughness values scaled by rows and columns
    // respectively
    for (int row = 0; row < nrRows; ++row) {
        //shader.setFloat("metallic", (float)row / (float)nrRows);
        //for (int col = 0; col < nrColumns; ++col) {
        //    // we clamp the roughness to 0.05 - 1.0 as perfectly smooth surfaces (roughness of 0.0) tend to look a bit
        //    // off on direct lighting.
        //    shader.setFloat("roughness", glm::clamp((float)col / (float)nrColumns, 0.05f, 1.0f));

        //    model = Math::float4x4(1.0f);
        //    model = glm::translate(model,
        //                           glm::vec3((col - (nrColumns / 2)) * spacing, (row - (nrRows / 2)) * spacing, 0.0f));
        //    shader.setMat4("model", model);
        //    renderSphere();
        //}
    }
}

void Pbr::run() {

    auto resource_uploaded_semaphore = rhi->GetSemaphore();
    bool resources_uploaded = false;


    for (;;) {

        //Horizon::RDC::StartFrameCapture();
        engine->m_input_system->Tick();

        rhi->AcquireNextFrame(swap_chain.get());
        camera_ub.vp = cam->GetViewProjectionMatrix();
        camera_ub.camera_pos = cam->GetPosition();
        camera_ub.exposure = cam->GetExposure();
        graphics_pass->SetGraphicsShader(vs, ps);
        //post_process_pass->SetComputeShader(cs);
        auto per_frame_descriptor_set = graphics_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);

        auto transfer = rhi->GetCommandList(CommandQueueType::TRANSFER);

        transfer->BeginRecording();
        transfer->UpdateBuffer(light_buffer.get(), directional_light->GetParamBuffer(), sizeof(LightParams));
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
        per_frame_descriptor_set->SetResource(camera_buffer.get(), 0);
        per_frame_descriptor_set->SetResource(light_buffer.get(), 2);
        per_frame_descriptor_set->SetResource(light_count_buffer.get(), 1);
        per_frame_descriptor_set->Update();

        // material descriptor set
        for (auto &node : mesh->GetNodes()) {
            if (node.mesh_primitives.empty()) {
                continue;
            }
            std::unordered_set<Material *> materials{};
            for (const auto &m : node.mesh_primitives) {
                materials.emplace(&mesh->GetMaterial(m->material_id));
            }

            for (auto &material : materials) {
                material->material_descriptor_set = graphics_pass->GetDescriptorSet(ResourceUpdateFrequency::PER_BATCH);

                // material.material_params.param_bitmask;
                // if (material->material_params.param_bitmask & HAS_BASE_COLOR) {
                material->material_descriptor_set->SetResource(
                    material->material_textures.at(MaterialTextureType::BASE_COLOR).texture.get(), 0);
                //}
                // if (material->material_params.param_bitmask & HAS_NORMAL) {
                material->material_descriptor_set->SetResource(
                    material->material_textures.at(MaterialTextureType::NORMAL).texture.get(), 1);
                //}
                // if (material->material_params.param_bitmask & HAS_METALLIC_ROUGHNESS) {
                material->material_descriptor_set->SetResource(
                    material->material_textures.at(MaterialTextureType::METALLIC_ROUGHTNESS).texture.get(), 2);
                //}
                material->material_descriptor_set->SetResource(sampler.get(), 3);
                material->material_descriptor_set->SetResource(material->param_buffer.get(), 4);
                material->material_descriptor_set->Update();
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

        cl->BeginRenderPass(begin_info);
        cl->BindPipeline(graphics_pass);

        u32 offset = 0;
        auto vb = mesh->GetVertexBuffer();
        cl->BindVertexBuffers(1, &vb, &offset);
        cl->BindIndexBuffer(mesh->GetIndexBuffer(), 0);

        cl->BindDescriptorSets(graphics_pass, per_frame_descriptor_set.get());

        for (auto &node : mesh->GetNodes()) {
            if (node.mesh_primitives.empty()) {
                continue;
            }
            auto mat = node.GetModelMatrix().Transpose();
            cl->BindPushConstant(graphics_pass, "model_matrix", &mat);

            for (auto &m : node.mesh_primitives) {
                auto &material = mesh->GetMaterial(m->material_id);
                cl->BindDescriptorSets(graphics_pass, material.material_descriptor_set.get());
                cl->DrawIndexedInstanced(m->index_count, m->index_offset, 0);
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
            QueuePresentInfo info{};
            info.swap_chain = swap_chain.get();
            rhi->Present(info);
        }
        //Horizon::RDC::EndFrameCapture();
    }
    rhi->DestroyPipeline(graphics_pass);
    //rhi->DestroyPipeline(post_process_pass);
    rhi->DestroyShader(vs);
    rhi->DestroyShader(ps);
    rhi->DestroyShader(cs);
    LOG_INFO("draw done");
}
