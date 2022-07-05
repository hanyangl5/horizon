#include <memory>
#include <string>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <iniparser.h>

#include <argparse/argparse.hpp>

#include <runtime/core/log/Log.h>
#include <runtime/core/window/Window.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/interface/EngineRuntime.h>
#include <runtime/system/input/InputSystem.h>
#include <runtime/system/render/RenderSystem.h>

using namespace Horizon;

class HorizonTest {
  public:
    HorizonTest() {
        EngineConfig config{};
        config.width = 800;
        config.height = 600;
        config.asset_path = "";
        config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
        config.offscreen = false;
        engine = std::make_unique<EngineRuntime>(config);
    }

  public:
    std::unique_ptr<EngineRuntime> engine{};
};

TEST_CASE_FIXTURE(HorizonTest, "buffer creation test") {
    engine->m_render_system->CreateBuffer(
        BufferCreateInfo{BufferUsage::BUFFER_USAGE_UNIFORM_BUFFER, 32});
}

TEST_CASE_FIXTURE(HorizonTest, "texture creation test") {
    engine->m_render_system->CreateTexture(TextureCreateInfo{
        TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM,
        TextureUsage::TEXTURE_USAGE_R, 4, 4, 1});
}

TEST_CASE_FIXTURE(HorizonTest, "buffer upload, dynamic") {

    Resource<Buffer> buffer{engine->m_render_system->CreateBuffer(
        BufferCreateInfo{BufferUsage::BUFFER_USAGE_UNIFORM_BUFFER |
                             BufferUsage::BUFFER_USAGE_DYNAMIC_UPDATE,
                         sizeof(Math::float3)})};

    // dynamic buffer, cpu pointer not change, cpu data change, gpu data
    // change
    for (u32 i = 0;i<10;i++)
    {
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
}

TEST_CASE_FIXTURE(HorizonTest, "buffer uploading, static") {

    Resource<Buffer> buffer{engine->m_render_system->CreateBuffer(
        BufferCreateInfo{BufferUsage::BUFFER_USAGE_UNIFORM_BUFFER,
                         sizeof(Math::float3)})};

    // dynamic buffer, cpu pointer not change, cpu data change, gpu data
    // change
    for (u32 i = 0; i < 3; i++) {

        engine->BeginNewFrame();

        Math::float3 data{0.0f};

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
}

TEST_CASE_FIXTURE(HorizonTest, "shader compile test") {
    std::string file_name =
        "D:/codes/horizon/horizon/assets/shaders/hlsl/shader.hlsl";
    auto shader_program = engine->m_render_system->CreateShaderProgram(
        ShaderType::VERTEX_SHADER, "vs_main", 0, file_name);
    engine->m_render_system->DestroyShaderProgram(shader_program);

}

TEST_CASE_FIXTURE(HorizonTest, "pipeline creation test") {

}

TEST_CASE_FIXTURE(HorizonTest, "multi thread command list recording") {}

TEST_CASE_FIXTURE(HorizonTest, "dispatch test") {

    std::string file_name =
        "D:/codes/horizon/horizon/assets/shaders/hlsl/cs.hlsl";
    auto shader{engine->m_render_system->CreateShaderProgram(
        ShaderType::COMPUTE_SHADER, "cs_main", 0, file_name)};
    auto pipeline{engine->m_render_system->CreatePipeline(
        PipelineCreateInfo{PipelineType::COMPUTE})};

    auto cl =
        engine->m_render_system->GetCommandList(CommandQueueType::COMPUTE);
    pipeline->SetShader(shader);
    cl->BeginRecording();
    cl->BindPipeline(pipeline);
    cl->Dispatch(1, 1, 1);
    cl->EndRecording();

    engine->m_render_system->SubmitCommandLists(COMPUTE, std::vector{cl});
}
