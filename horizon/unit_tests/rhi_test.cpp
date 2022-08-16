#include "config.h"

#include <doctest/doctest.h>

namespace TEST::RHI {
using namespace Horizon;

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
    }

  public:
    std::unique_ptr<Engine> engine{};
    std::string asset_path = "D:/codes/horizon/horizon/assets/";
};

TEST_CASE_FIXTURE(RHITest, "buffer creation test") {
    engine->m_render_system->CreateBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                         ResourceState::RESOURCE_STATE_SHADER_RESOURCE, 32});
}

TEST_CASE_FIXTURE(RHITest, "texture creation test") {
    engine->m_render_system->CreateTexture(
        TextureCreateInfo{DescriptorType::DESCRIPTOR_TYPE_TEXTURE,
                          ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                          TextureType::TEXTURE_TYPE_2D,
                          TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, 4, 4, 1});
}

TEST_CASE_FIXTURE(RHITest, "buffer upload, dynamic") {

    Resource<Buffer> buffer{engine->m_render_system->CreateBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                         ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                         sizeof(Math::float3)})};

    // dynamic buffer, cpu pointer not change, cpu data change, gpu data
    // change
    for (u32 i = 0; i < 10; i++) {
        engine->BeginNewFrame();

        Math::float3 data{
            static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
            static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
            static_cast<float>(rand()) / static_cast<float>(RAND_MAX)};

        auto transfer =
            engine->m_render_system->GetCommandList(CommandQueueType::TRANSFER);

        transfer->BeginRecording();

        // cpu -> stage
        transfer->UpdateBuffer(buffer.get(), &data, sizeof(data));

        transfer->EndRecording();

        // stage -> gpu
        engine->m_render_system->SubmitCommandLists(CommandQueueType::TRANSFER,
                                                    std::vector{transfer});
    }
    engine->BeginNewFrame();
}

TEST_CASE_FIXTURE(RHITest, "shader compile test") {
    std::string file_name = asset_path + "shaders/hlsl/shader.hlsl";
    auto shader_program = engine->m_render_system->CreateShaderProgram(
        ShaderType::VERTEX_SHADER, "vs_main", 0, file_name);
    engine->m_render_system->DestroyShaderProgram(shader_program);
}

TEST_CASE_FIXTURE(RHITest, "spirv shader reflection test") {
    std::string file_name = asset_path + "shaders/hlsl/"
                                         "ps_descriptor_set_reflect.hlsl";
    auto shader_program = engine->m_render_system->CreateShaderProgram(
        ShaderType::COMPUTE_SHADER, "cs_main", 0, file_name);
    engine->m_render_system->DestroyShaderProgram(shader_program);
}

TEST_CASE_FIXTURE(RHITest, "pipeline creation test") {}

TEST_CASE_FIXTURE(RHITest, "dispatch test") {
    // Horizon::RDC::StartFrameCapture();
    std::string file_name = asset_path + "shaders/hlsl/cs.hlsl";
    auto shader{engine->m_render_system->CreateShaderProgram(
        ShaderType::COMPUTE_SHADER, "cs_main", 0, file_name)};
    ComputePipelineCreateInfo info;
    auto pipeline = engine->m_render_system->CreateComputePipeline(info);

    auto cl =
        engine->m_render_system->GetCommandList(CommandQueueType::COMPUTE);
    pipeline->SetComputeShader(shader);
    cl->BeginRecording();
    cl->BindPipeline(pipeline);
    cl->Dispatch(1, 1, 1);
    cl->EndRecording();

    engine->m_render_system->SubmitCommandLists(COMPUTE, std::vector{cl});
    // Horizon::RDC::EndFrameCapture();
    engine->BeginNewFrame();
}

TEST_CASE_FIXTURE(RHITest, "descriptor set cache") {
    Horizon::RDC::StartFrameCapture();
    auto &rs = engine->m_render_system;
    std::string file_name =
        asset_path + "shaders/hlsl/cs_descriptor_set_cache.hlsl";

    auto shader = engine->m_render_system->CreateShaderProgram(
        ShaderType::COMPUTE_SHADER, "cs_main", 0, file_name);

    ComputePipelineCreateInfo info;
    auto pipeline = engine->m_render_system->CreateComputePipeline(info);

    Resource<Buffer> cb1{engine->m_render_system->CreateBuffer(BufferCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
        ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(f32)})};

    Resource<Buffer> cb2{engine->m_render_system->CreateBuffer(BufferCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
        ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(f32)})};
    Resource<Buffer> cb3{engine->m_render_system->CreateBuffer(BufferCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
        ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(f32)})};

    Resource<Buffer> cb4{engine->m_render_system->CreateBuffer(BufferCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
        ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(f32)})};
    Resource<Buffer> rwb1{engine->m_render_system->CreateBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_RW_BUFFER,
                         ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                         sizeof(f32)})};

    Resource<Buffer> rwb2{engine->m_render_system->CreateBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_RW_BUFFER,
                         ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                         sizeof(f32)})};
    Resource<Buffer> rwb3{engine->m_render_system->CreateBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_RW_BUFFER,
                         ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                         sizeof(f32)})};

    Resource<Buffer> rwb4{engine->m_render_system->CreateBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_RW_BUFFER,
                         ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                         sizeof(f32)})};

    f32 data[4] = {5, 6, 7, 7};

    auto transfer =
        engine->m_render_system->GetCommandList(CommandQueueType::TRANSFER);

    transfer->BeginRecording();

    // cpu -> stage
    transfer->UpdateBuffer(cb1.get(), &data[0], sizeof(f32)); // 5

    transfer->UpdateBuffer(cb2.get(), &data[1], sizeof(f32)); // 6

    transfer->UpdateBuffer(cb3.get(), &data[2], sizeof(f32)); // 7

    transfer->UpdateBuffer(cb4.get(), &data[3], sizeof(f32)); // 7

    transfer->EndRecording();

    engine->m_render_system->SubmitCommandLists(CommandQueueType::TRANSFER,
                                                std::vector{transfer});

    auto cl =
        engine->m_render_system->GetCommandList(CommandQueueType::COMPUTE);

    // barrier trans -> compute

    pipeline->SetComputeShader(shader);

    rs->SetResource(cb1.get(), pipeline, 0, 0); // set, binding
    rs->SetResource(cb2.get(), pipeline, 0, 1);
    rs->SetResource(cb3.get(), pipeline, 2, 0); // set, binding
    rs->SetResource(cb4.get(), pipeline, 2, 1);
    rs->SetResource(rwb1.get(), pipeline, 1, 0); // set, binding
    rs->SetResource(rwb2.get(), pipeline, 1, 1);
    rs->SetResource(rwb3.get(), pipeline, 3, 0); // set, binding
    rs->SetResource(rwb4.get(), pipeline, 3, 1);

    cl->BeginRecording();
    cl->BindPipeline(pipeline);

    cl->Dispatch(1, 1, 1);
    cl->EndRecording();

    engine->m_render_system->SubmitCommandLists(COMPUTE, std::vector{cl});

    Horizon::RDC::EndFrameCapture();
    engine->BeginNewFrame();
}

TEST_CASE_FIXTURE(RHITest, "bindless descriptors") {

    // Horizon::RDC::StartFrameCapture();
    // std::string file_name = "D:/codes/horizon/horizon/assets/shaders/hlsl/"
    //                         "cs_bindless_descriptor.hlsl";
    // auto shader{engine->m_render_system->CreateShaderProgram(
    //     ShaderType::COMPUTE_SHADER, "cs_main", 0, file_name)};
    // auto pipeline{engine->m_render_system->CreatePipeline(
    //     PipelineCreateInfo{PipelineType::COMPUTE})};

    // auto buffer = engine->m_render_system->CreateBuffer(
    //     BufferCreateInfo{BufferUsage::BUFFER_USAGE_UNIFORM_BUFFER |
    //                          BufferUsage::BUFFER_USAGE_DYNAMIC_UPDATE,
    //                      sizeof(Math::float4)});
    ////auto texture;
    // Math::float4 data{5.0f};

    // auto transfer =
    //     engine->m_render_system->GetCommandList(CommandQueueType::TRANSFER);

    // transfer->BeginRecording();

    //// cpu -> stage
    // transfer->UpdateBuffer(buffer.get(), &data, sizeof(data));

    // transfer->EndRecording();

    //// stage -> gpu
    // engine->m_render_system->SubmitCommandLists(CommandQueueType::TRANSFER,
    //                                             std::vector{transfer});

    // auto cl =
    //     engine->m_render_system->GetCommandList(CommandQueueType::COMPUTE);
    // pipeline->SetShader(shader);
    // engine->m_render_system->SetResource(buffer.get());
    // engine->m_render_system->UpdateDescriptors();
    // cl->BeginRecording();
    // cl->BindPipeline(pipeline);
    // cl->Dispatch(1, 1, 1);
    // cl->EndRecording();

    // engine->m_render_system->SubmitCommandLists(COMPUTE, std::vector{cl});
    // Horizon::RDC::EndFrameCapture();
}

TEST_CASE_FIXTURE(RHITest, "multi thread command list recording") {
    auto &tp = engine->tp;
    auto &rs = engine->m_render_system;

    constexpr u32 cmdlist_count = 20;
    std::vector<CommandList *> cmdlists(cmdlist_count);
    std::vector<std::future<void>> results(cmdlist_count);

    std::vector<Resource<Buffer>> buffers;

    for (u32 i = 0; i < cmdlist_count; i++) {
        buffers.emplace_back(engine->m_render_system->CreateBuffer(
            BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                             ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                             sizeof(Math::float3)}));

        results[i] = std::move(tp->submit([&rs, &cmdlists, &buffers, i]() {
            Math::float3 data{
                static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
                static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
                static_cast<float>(rand()) / static_cast<float>(RAND_MAX)};

            auto transfer = rs->GetCommandList(CommandQueueType::TRANSFER);

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

    engine->m_render_system->SubmitCommandLists(CommandQueueType::TRANSFER,
                                                cmdlists);

    engine->BeginNewFrame();
}

TEST_CASE_FIXTURE(RHITest, "graphics pipeline") {}

TEST_CASE_FIXTURE(RHITest, "draw") {}

} // namespace TEST::RHI