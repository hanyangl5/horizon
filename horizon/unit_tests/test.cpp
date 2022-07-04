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

TEST_CASE_FIXTURE(HorizonTest, "shader compile test") {
    std::string file_name =
        "D:/codes/horizon/horizon/assets/shaders/hlsl/shader.hlsl";
    auto shader_program = engine->m_render_system->CreateShaderProgram(
        ShaderType::VERTEX_SHADER, "vs_main", 0, file_name);
    engine->m_render_system->DestroyShaderProgram(shader_program);

    // destroy
}

TEST_CASE_FIXTURE(HorizonTest, "pipeline creation test") {}

TEST_CASE_FIXTURE(HorizonTest, "command list test") {}

TEST_CASE_FIXTURE(HorizonTest, "dispatch test") {}
