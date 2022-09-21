#include "config.h"

#include <doctest/doctest.h>

namespace TEST {

using namespace Horizon;
using namespace Horizon::RHI;

class RHITest {
  public:
    RHITest() {
        EngineConfig config{};
        config.width = 800;
        config.height = 600;
        // config.asset_path =
        config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
        config.offscreen = false;
        engine = std::make_unique<Engine>(config);

        width = config.width;
        height = config.height;
    }

  public:
    std::unique_ptr<Engine> engine{};
    std::string asset_path = "C:/FILES/horizon/horizon/assets/";
    u32 width, height;
};

TEST_CASE_FIXTURE(RHITest, "buffer creation test") {

    auto rhi = engine->m_render_system->GetRhi();
    rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                       ResourceState::RESOURCE_STATE_SHADER_RESOURCE, 32});
}

TEST_CASE_FIXTURE(RHITest, "texture creation test") {

    auto rhi = engine->m_render_system->GetRhi();
    rhi->CreateTexture(TextureCreateInfo{DescriptorType::DESCRIPTOR_TYPE_TEXTURE,
                                         ResourceState::RESOURCE_STATE_SHADER_RESOURCE, TextureType::TEXTURE_TYPE_2D,
                                         TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, 4, 4, 1});
}

TEST_CASE_FIXTURE(RHITest, "buffer upload, dynamic") {

    auto rhi = engine->m_render_system->GetRhi();

    Resource<Buffer> buffer =
        rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                           ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(Math::float3)});

    // dynamic buffer, cpu pointer not change, cpu data change, gpu data
    // change
    for (u32 i = 0; i < 10; i++) {
        engine->BeginNewFrame();
        Math::float3 data{static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
                          static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
                          static_cast<float>(rand()) / static_cast<float>(RAND_MAX)};

        auto transfer = rhi->GetCommandList(CommandQueueType::TRANSFER);

        transfer->BeginRecording();

        // cpu -> stage
        transfer->UpdateBuffer(buffer.get(), &data, sizeof(data));

        transfer->EndRecording();

        // stage -> gpu
        QueueSubmitInfo submit_info{};
        submit_info.queue_type = CommandQueueType::TRANSFER;
        submit_info.command_lists.push_back(transfer);

        rhi->SubmitCommandLists(submit_info);
        engine->EndFrame();
    }
}

TEST_CASE_FIXTURE(RHITest, "shader compile test") {

    auto rhi = engine->m_render_system->GetRhi();
    std::string file_name = asset_path + "shaders/cs.comp.hsl";
    auto shader_program = rhi->CreateShader(ShaderType::VERTEX_SHADER, 0, file_name);
    rhi->DestroyShader(shader_program);
}

//TEST_CASE_FIXTURE(RHITest, "binding model") {
//
//    auto rhi = engine->m_render_system->GetRhi();
//    Horizon::RDC::StartFrameCapture();
//    std::string file_name = asset_path + "shaders/shader_resource_frequency.comp.hsl";
//
//    auto shader = rhi->CreateShader(ShaderType::COMPUTE_SHADER, 0, file_name);
//
//    Math::float4 f = Math::float4(5.0);
//
//    ComputePipelineCreateInfo info;
//    auto pipeline = rhi->CreateComputePipeline(info);
//
//    Resource<Buffer> cb1{rhi->CreateBuffer(BufferCreateInfo{
//        DescriptorType::DESCRIPTOR_TYPE_RW_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(f32)})};
//
//    Resource<Buffer> cb2{rhi->CreateBuffer(BufferCreateInfo{
//        DescriptorType::DESCRIPTOR_TYPE_RW_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(f32)})};
//
//    Resource<Buffer> rwb1{rhi->CreateBuffer(BufferCreateInfo{
//        DescriptorType::DESCRIPTOR_TYPE_RW_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(f32)})};
//
//    Resource<Buffer> rwb2{rhi->CreateBuffer(BufferCreateInfo{
//        DescriptorType::DESCRIPTOR_TYPE_RW_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(f32)})};
//    Resource<Buffer> rwb3{rhi->CreateBuffer(BufferCreateInfo{
//        DescriptorType::DESCRIPTOR_TYPE_RW_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(f)})};
//
//    Resource<Buffer> rwb4{rhi->CreateBuffer(BufferCreateInfo{
//        DescriptorType::DESCRIPTOR_TYPE_RW_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(f)})};
//
//    f32 data[4] = {5, 6, 7, 7};
//
//    auto transfer = rhi->GetCommandList(CommandQueueType::TRANSFER);
//
//    transfer->BeginRecording();
//
//    // cpu -> stage
//    transfer->UpdateBuffer(cb1.get(), &data[0], sizeof(f32)); // 5
//
//    transfer->UpdateBuffer(cb2.get(), &data[1], sizeof(f32)); // 6
//
//    transfer->EndRecording();
//
//    QueueSubmitInfo submit_info{};
//    submit_info.queue_type = CommandQueueType::TRANSFER;
//    submit_info.command_lists.push_back(transfer);
//    auto s1 = rhi->GetSemaphore();
//    submit_info.signal_semaphores.push_back(s1.get());
//    rhi->SubmitCommandLists(submit_info);
//
//    auto cl = rhi->GetCommandList(CommandQueueType::COMPUTE);
//
//    // barrier trans -> compute
//
//    pipeline->SetComputeShader(shader);
//
//    pipeline->BindResource(cb1.get(), ResourceUpdateFrequency::NONE, 0); // set, binding
//    pipeline->BindResource(cb2.get(), ResourceUpdateFrequency::NONE, 1);
//    pipeline->BindResource(rwb1.get(), ResourceUpdateFrequency::PER_FRAME, 0); // set, binding
//    pipeline->BindResource(rwb2.get(), ResourceUpdateFrequency::PER_FRAME, 1);
//    pipeline->BindResource(rwb3.get(), ResourceUpdateFrequency::PER_DRAW, 0); // set, binding
//    pipeline->BindResource(rwb4.get(), ResourceUpdateFrequency::PER_DRAW, 1);
//
//    cl->BeginRecording();
//
//    pipeline->UpdatePipelineDescriptorSet(ResourceUpdateFrequency::NONE);
//    pipeline->UpdatePipelineDescriptorSet(ResourceUpdateFrequency::PER_FRAME);
//    pipeline->UpdatePipelineDescriptorSet(ResourceUpdateFrequency::PER_DRAW);
//
//    cl->BindPipeline(pipeline);
//
//    cl->Dispatch(1, 1, 1);
//    cl->EndRecording();
//
//    QueueSubmitInfo submit_info2{};
//    submit_info2.queue_type = CommandQueueType::COMPUTE;
//    submit_info2.command_lists.push_back(cl);
//    submit_info2.wait_semaphores.push_back(s1.get());
//    rhi->SubmitCommandLists(submit_info2);
//
//    Horizon::RDC::EndFrameCapture();
//    engine->EndFrame();
//
//    rhi->DestroyShader(shader);
//    rhi->DestroyPipeline(pipeline);
//}

// TEST_CASE_FIXTURE(RHITest, "bindless descriptors") {
//
//     // Horizon::RDC::StartFrameCapture();
//     // std::string file_name = "D:/codes/horizon/horizon/assets/shaders/hlsl/"
//     //                         "cs_bindless_descriptor.hlsl";
//     // auto shader{rhi->CreateShader(
//     //     ShaderType::COMPUTE_SHADER, "cs_main", 0, file_name)};
//     // auto pipeline{rhi->CreatePipeline(
//     //     PipelineCreateInfo{PipelineType::COMPUTE})};
//
//     // auto buffer = rhi->CreateBuffer(
//     //     BufferCreateInfo{BufferUsage::BUFFER_USAGE_UNIFORM_BUFFER |
//     //                          BufferUsage::BUFFER_USAGE_DYNAMIC_UPDATE,
//     //                      sizeof(Math::float4)});
//     ////auto texture;
//     // Math::float4 data{5.0f};
//
//     // auto transfer =
//     //     rhi->GetCommandList(CommandQueueType::TRANSFER);
//
//     // transfer->BeginRecording();
//
//     //// cpu -> stage
//     // transfer->UpdateBuffer(buffer.get(), &data, sizeof(data));
//
//     // transfer->EndRecording();
//
//     //// stage -> gpu
//     // rhi->SubmitCommandLists(CommandQueueType::TRANSFER,
//     //                                             std::vector{transfer});
//
//     // auto cl =
//     //     rhi->GetCommandList(CommandQueueType::COMPUTE);
//     // pipeline->SetShader(shader);
//     // rhi->SetResource(buffer.get());
//     // rhi->UpdateDescriptors();
//     // cl->BeginRecording();
//     // cl->BindPipeline(pipeline);
//     // cl->Dispatch(1, 1, 1);
//     // cl->EndRecording();
//
//     // rhi->SubmitCommandLists(COMPUTE, std::vector{cl});
//     // Horizon::RDC::EndFrameCapture();
// }

TEST_CASE_FIXTURE(RHITest, "multi thread command list recording") {
    auto &tp = engine->tp;
    auto rhi = engine->m_render_system->GetRhi();
    constexpr u32 cmdlist_count = 20;
    std::vector<CommandList *> cmdlists(cmdlist_count);
    std::vector<std::future<void>> results(cmdlist_count);

    std::vector<Resource<Buffer>> buffers;

    for (u32 i = 0; i < cmdlist_count; i++) {
        buffers.emplace_back(
            rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                               ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(Math::float3)}));

        results[i] = std::move(tp->submit([&rhi, &cmdlists, &buffers, i]() {
            Math::float3 data{static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
                              static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
                              static_cast<float>(rand()) / static_cast<float>(RAND_MAX)};

            auto transfer = rhi->GetCommandList(CommandQueueType::TRANSFER);

            transfer->BeginRecording();

            // cpu -> stage
            transfer->UpdateBuffer(buffers[i].get(), &data,
                                   sizeof(data)); // TODO: multithread

            transfer->EndRecording();
            // stage -> gpu
            cmdlists[i] = transfer;
        }));
    }

    for (auto &res : results) {
        res.wait();
    }
    LOG_INFO("all task done");

    QueueSubmitInfo submit_info{};
    submit_info.queue_type = CommandQueueType::TRANSFER;
    submit_info.command_lists.swap(cmdlists);
    rhi->SubmitCommandLists(submit_info);

    rhi->WaitGpuExecution(CommandQueueType::TRANSFER);
}

//TEST_CASE_FIXTURE(RHITest, "draw test") {
//
//    auto rhi = engine->m_render_system->GetRhi();
//    std::string vs_path = asset_path + "shaders/draw.vert.hsl";
//    std::string ps_path = asset_path + "shaders/draw.frag.hsl";
//
//    auto vs = rhi->CreateShader(ShaderType::VERTEX_SHADER, 0, vs_path);
//
//    auto ps = rhi->CreateShader(ShaderType::PIXEL_SHADER, 0, ps_path);
//
//    auto rt0 = rhi->CreateRenderTarget(
//        RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM, RenderTargetType::COLOR, width, height});
//    auto depth = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_D32_SFLOAT,
//                                                                RenderTargetType::DEPTH_STENCIL, width, height});
//
//    GraphicsPipelineCreateInfo info{};
//
//    info.vertex_input_state.attribute_count = 4;
//
//    auto &pos = info.vertex_input_state.attributes[0];
//    pos.attrib_format = VertexAttribFormat::F32; // position
//    pos.portion = 3;
//    pos.binding = 0;
//    pos.location = 0;
//    pos.offset = 0;
//    pos.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
//
//    auto &normal = info.vertex_input_state.attributes[1];
//    normal.attrib_format = VertexAttribFormat::F32; // normal, TOOD: SN16 is a better format
//    normal.portion = 3;
//    normal.binding = 0;
//    normal.location = 1;
//    normal.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
//    normal.offset = offsetof(Vertex, normal);
//
//    auto &uv0 = info.vertex_input_state.attributes[2];
//    uv0.attrib_format = VertexAttribFormat::F32; // uv0 TOOD: UN16 is a better format
//    uv0.portion = 2;
//    uv0.binding = 0;
//    uv0.location = 2;
//    uv0.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
//    uv0.offset = offsetof(Vertex, uv0);
//
//    auto &uv1 = info.vertex_input_state.attributes[3];
//    uv1.attrib_format = VertexAttribFormat::F32; // uv1 TOOD: UN16 is a better format
//    uv1.portion = 2;
//    uv1.binding = 0;
//    uv1.location = 3;
//    uv1.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
//    uv1.offset = offsetof(Vertex, uv1);
//
//    info.view_port_state.width = width;
//    info.view_port_state.height = height;
//
//    info.depth_stencil_state.depth_func = DepthFunc::LESS;
//    info.depth_stencil_state.depthNear = 0.0f;
//    info.depth_stencil_state.depthNear = 1.0f;
//    info.depth_stencil_state.depth_test = true;
//    info.depth_stencil_state.depth_write = true;
//    info.depth_stencil_state.stencil_enabled = false;
//
//    info.input_assembly_state.topology = PrimitiveTopology::TRIANGLE_LIST;
//
//    info.multi_sample_state.sample_count = 1;
//
//    info.rasterization_state.cull_mode = CullMode::BACK;
//    info.rasterization_state.discard = false;
//    info.rasterization_state.fill_mode = FillMode::TRIANGLE;
//    info.rasterization_state.front_face = FrontFace::CCW;
//
//    info.render_target_formats.color_attachment_count = 1;
//    info.render_target_formats.color_attachment_formats =
//        std::vector<TextureFormat>{TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM};
//    info.render_target_formats.has_depth = true;
//    info.render_target_formats.depth_stencil_format = TextureFormat::TEXTURE_FORMAT_D32_SFLOAT;
//
//    auto pipeline = rhi->CreateGraphicsPipeline(info);
//
//    Mesh mesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL | VertexAttributeType::UV0});
//    //mesh.LoadMesh(asset_path + "models/DamagedHelmet/DamagedHelmet.gltf");
//    mesh.LoadMesh(asset_path + "models/FlightHelmet/glTF/FlightHelmet.gltf");
//    //mesh.LoadMesh(asset_path + "models/Sponza/glTF/Sponza.gltf");
//    mesh.CreateGpuResources(rhi);
//
//
//    auto view =
//        Math::LookAt(Math::float3(0.0f, 0.0f, 1.0f), Math::float3(0.0f, 0.0f, 0.0f), Math::float3(0.0f, 1.0f, 0.0f));
//
//    auto projection = Math::Perspective(90.0f, (float)width / (float)height, 0.1f, 100.0f);
//
//    // row major
//    auto vp = view * projection;
//    vp = vp.Transpose();
//    auto vp_buffer = rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
//                                                        ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(vp)});
//
//    pipeline->SetGraphicsShader(vs, ps);
//
//    auto image_acquired_semaphore = rhi->GetSemaphore();
//
//    for (u32 frame = 0; frame < 3; frame++) {
//        engine->BeginNewFrame();
//        Horizon::RDC::StartFrameCapture();
//
//        rhi->AcquireNextImage(image_acquired_semaphore.get(), frame);
//
//        auto transfer = rhi->GetCommandList(CommandQueueType::TRANSFER);
//
//        transfer->BeginRecording();
//
//        transfer->UpdateBuffer(vp_buffer.get(), &vp, sizeof(Math::float4x4));
//        
//        mesh.UploadResources(transfer);
//
//
//        transfer->EndRecording();
//
//        auto resource_uploaded_semaphore = rhi->GetSemaphore();
//        {
//            QueueSubmitInfo submit_info{};
//            submit_info.queue_type = CommandQueueType::TRANSFER;
//            submit_info.command_lists.push_back(transfer);
//            submit_info.signal_semaphores.push_back(resource_uploaded_semaphore.get());
//            rhi->SubmitCommandLists(submit_info);
//        }
//
//        auto cl = rhi->GetCommandList(CommandQueueType::GRAPHICS);
//
//        pipeline->BindResource(vp_buffer.get(), ResourceUpdateFrequency::PER_FRAME, 0);
//
//        pipeline->UpdatePipelineDescriptorSet(ResourceUpdateFrequency::PER_FRAME);
//        cl->BeginRecording();
//
//        RenderPassBeginInfo begin_info{};
//        begin_info.render_area = Rect{0, 0, width, height};
//        begin_info.render_targets[0].data = rt0.get();
//        begin_info.render_targets[0].clear_color = ClearValueColor{Math::float4(0.0)};
//        begin_info.depth_stencil.data = depth.get();
//        begin_info.depth_stencil.clear_color = ClearValueDepthStencil{1.0, 0};
//
//        cl->BeginRenderPass(begin_info);
//
//        cl->BindPipeline(pipeline);
//
//        u32 offset = 0;
//        auto vb = mesh.GetVertexBuffer();
//        cl->BindVertexBuffers(1, &vb, &offset);
//        cl->BindIndexBuffer(mesh.GetIndexBuffer(), 0);
//
//
//        for (auto &node : mesh.GetNodes()) {
//            if (node.mesh_primitives.empty()) {
//                continue;
//            }
//            auto mat = node.GetModelMatrix().Transpose();
//            cl->BindPushConstant(pipeline, "model_matrix", &mat);
//            for (auto &m : node.mesh_primitives) {
//                cl->DrawIndexedInstanced(m->index_count, m->index_offset, 0);
//            }
//        }
//
//        cl->EndRenderPass();
//
//        cl->EndRecording();
//
//        auto render_complete_semaphore = rhi->GetSemaphore();
//        {
//            QueueSubmitInfo submit_info{};
//            submit_info.queue_type = CommandQueueType::GRAPHICS;
//            submit_info.command_lists.push_back(cl);
//            submit_info.wait_semaphores.push_back(resource_uploaded_semaphore.get());
//            submit_info.wait_semaphores.push_back(image_acquired_semaphore.get());
//            submit_info.signal_semaphores.push_back(render_complete_semaphore.get());
//            rhi->SubmitCommandLists(submit_info);
//        }
//
//        //{
//        //    QueuePresentInfo info{};
//        //    info.wait_semaphores.push_back(render_complete_semaphore.get());
//
//        //    rhi->Present(info);
//        //}
//        Horizon::RDC::EndFrameCapture();
//        engine->EndFrame();
//    }
//
//    rhi->DestroyPipeline(pipeline);
//    rhi->DestroyShader(vs);
//    rhi->DestroyShader(ps);
//    LOG_INFO("draw done");
//}

TEST_CASE_FIXTURE(RHITest, "texture and material") {

    auto rhi = engine->m_render_system->GetRhi();
    std::string vs_path = asset_path + "shaders/draw_material.vert.hsl";
    std::string ps_path = asset_path + "shaders/draw_material.frag.hsl";

    auto vs = rhi->CreateShader(ShaderType::VERTEX_SHADER, 0, vs_path);

    auto ps = rhi->CreateShader(ShaderType::PIXEL_SHADER, 0, ps_path);

    auto rt0 = rhi->CreateRenderTarget(
        RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM, RenderTargetType::COLOR, width, height});
    auto depth = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_D32_SFLOAT,
                                                                RenderTargetType::DEPTH_STENCIL, width, height});

    GraphicsPipelineCreateInfo info{};

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
    info.render_target_formats.color_attachment_formats =
        std::vector<TextureFormat>{TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM};
    info.render_target_formats.has_depth = true;
    info.render_target_formats.depth_stencil_format = TextureFormat::TEXTURE_FORMAT_D32_SFLOAT;

    auto pipeline = rhi->CreateGraphicsPipeline(info);

    Mesh mesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL | VertexAttributeType::UV0});
    //mesh.LoadMesh(asset_path + "models/DamagedHelmet/DamagedHelmet.gltf");
    mesh.LoadMesh(asset_path + "models/FlightHelmet/glTF/FlightHelmet.gltf");
    // mesh.LoadMesh(asset_path + "models/Sponza/glTF/Sponza.gltf");
    mesh.CreateGpuResources(rhi);

    SamplerDesc sampler_desc{};
    sampler_desc.min_filter = FilterType::FILTER_LINEAR;
    sampler_desc.mag_filter = FilterType::FILTER_LINEAR;
    sampler_desc.mip_map_mode = MipMapMode::MIPMAP_MODE_LINEAR;
    sampler_desc.address_u = AddressMode::ADDRESS_MODE_REPEAT;
    sampler_desc.address_v = AddressMode::ADDRESS_MODE_REPEAT;
    sampler_desc.address_w = AddressMode::ADDRESS_MODE_REPEAT;

    auto sampler = rhi->GetSampler(sampler_desc);
    auto view =
        Math::LookAt(Math::float3(0.0f, 0.0f, 1.0f), Math::float3(0.0f, 0.0f, 0.0f), Math::float3(0.0f, 1.0f, 0.0f));

    auto projection = Math::Perspective(90.0f, (float)width / (float)height, 0.1f, 100.0f);

    // row major
    auto vp = view * projection;
    vp = vp.Transpose();
    auto vp_buffer = rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                                        ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(vp)});

    auto image_acquired_semaphore = rhi->GetSemaphore();
    auto resource_uploaded_semaphore = rhi->GetSemaphore();
    bool resources_uploaded = false;

    for (u32 frame = 0; frame < 3; frame++) {

        engine->BeginNewFrame();
        Horizon::RDC::StartFrameCapture();

        rhi->AcquireNextImage(image_acquired_semaphore.get(), frame);

        pipeline->SetGraphicsShader(vs, ps);

        
        auto per_frame_descriptor_set = pipeline->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);

        auto transfer = rhi->GetCommandList(CommandQueueType::TRANSFER);

        transfer->BeginRecording();
        
        transfer->UpdateBuffer(vp_buffer.get(), &vp, sizeof(Math::float4x4));

        if (!resources_uploaded) {
            resources_uploaded = true;
            mesh.UploadResources(transfer);
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
        per_frame_descriptor_set->SetResource(vp_buffer.get(), 0);
        per_frame_descriptor_set->Update();

        // material descriptor set
        for (auto &node : mesh.GetNodes()) {
            if (node.mesh_primitives.empty()) {
                continue;
            }
            for (const auto &m : node.mesh_primitives) {
                auto &material = mesh.GetMaterial(m->material_id);
                material.material_descriptor_set = pipeline->GetDescriptorSet(ResourceUpdateFrequency::PER_DRAW);

                material.material_descriptor_set->SetResource(
                    material.material_textures.at(MaterialTextureType::BASE_COLOR).texture.get(), 0);
                material.material_descriptor_set->SetResource(
                    material.material_textures.at(MaterialTextureType::NORMAL).texture.get(), 1);
                material.material_descriptor_set->SetResource(
                    material.material_textures.at(MaterialTextureType::METALLIC_ROUGHTNESS).texture.get(), 2);
                material.material_descriptor_set->SetResource(sampler.get(), 3);

                material.material_descriptor_set->Update();
            }
        }

        auto cl = rhi->GetCommandList(CommandQueueType::GRAPHICS);
        cl->BeginRecording();

        RenderPassBeginInfo begin_info{};
        begin_info.render_area = Rect{0, 0, width, height};
        begin_info.render_targets[0].data = rt0.get();
        begin_info.render_targets[0].clear_color = ClearValueColor{Math::float4(0.0)};
        begin_info.depth_stencil.data = depth.get();
        begin_info.depth_stencil.clear_color = ClearValueDepthStencil{1.0, 0};

        cl->BeginRenderPass(begin_info);
        cl->BindPipeline(pipeline);

        u32 offset = 0;
        auto vb = mesh.GetVertexBuffer();
        cl->BindVertexBuffers(1, &vb, &offset);
        cl->BindIndexBuffer(mesh.GetIndexBuffer(), 0);

        cl->BindDescriptorSets(pipeline, per_frame_descriptor_set.get());
       
        for (auto &node : mesh.GetNodes()) {
            if (node.mesh_primitives.empty()) {
                continue;
            }
            auto mat = node.GetModelMatrix().Transpose();
            cl->BindPushConstant(pipeline, "model_matrix", &mat);

            for (auto &m : node.mesh_primitives) {
                auto& material = mesh.GetMaterial(m->material_id);
                cl->BindDescriptorSets(pipeline, material.material_descriptor_set.get());
                cl->DrawIndexedInstanced(m->index_count, m->index_offset, 0);
            }
        }

        cl->EndRenderPass();

        cl->EndRecording();

        auto render_complete_semaphore = rhi->GetSemaphore();
        {
            QueueSubmitInfo submit_info{};
            submit_info.queue_type = CommandQueueType::GRAPHICS;
            submit_info.command_lists.push_back(cl);
            submit_info.wait_semaphores.push_back(resource_uploaded_semaphore.get());
            submit_info.wait_semaphores.push_back(image_acquired_semaphore.get());
            submit_info.signal_semaphores.push_back(render_complete_semaphore.get());
            rhi->SubmitCommandLists(submit_info);
        }

        //{
        //    QueuePresentInfo info{};
        //    info.wait_semaphores.push_back(render_complete_semaphore.get());

        //    rhi->Present(info);
        //}
        Horizon::RDC::EndFrameCapture();
        engine->EndFrame();
    }

    rhi->DestroyPipeline(pipeline);
    rhi->DestroyShader(vs);
    rhi->DestroyShader(ps);
    LOG_INFO("draw done");
   
}

} // namespace TEST