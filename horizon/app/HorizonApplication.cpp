#include <memory>
#include <string>

#include <iniparser.h>

#include <argparse/argparse.hpp>

#include <runtime/core/log/Log.h>
#include <runtime/core/window/Window.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/interface/Engine.h>
#include <runtime/system/input/InputSystem.h>
#include <runtime/system/render/RenderSystem.h>

using namespace Horizon;

class HorizonApplication {
  public:
    void Run(const EngineConfig &config) noexcept;
    void PrepareResources() noexcept;

  private:
    std::unique_ptr<Engine> engine{};
};

void HorizonApplication::Run(const EngineConfig &config) noexcept {
    engine = std::make_unique<Engine>(config);

    PrepareResources();

    while (!engine->m_window->ShouldClose()) {
        engine->BeginNewFrame();
        // engine->m_render_system->CreateBuffer({});
        // engine->m_render_system->
    }
}

void HorizonApplication::PrepareResources() noexcept {}

int main(int argc, char *argv[]) {
    std::unique_ptr<HorizonApplication> application{
        std::make_unique<HorizonApplication>()};

    argparse::ArgumentParser args("app");

    args.add_argument("-config_path").help("engine config path");

    args.parse_args(argc, argv);

    const auto &config_path = args.get<std::string>("-config_path");

    // read ini file
    dictionary *ini{iniparser_load(config_path.c_str())};
    if (!ini) {
        LOG_ERROR("cannot load config file: {}", config_path);
        return 1;
    }

    EngineConfig config{};

    config.width = iniparser_getint(ini, "app:width", 1920);
    config.height = iniparser_getint(ini, "app:height", 1080);

    const auto &asset_path = iniparser_getstring(
        ini, "app:asset_path",
        "UNKOWN"); // TODO: convert to absolute path, cmake ?

    if (asset_path == "UNKOWN") {
        LOG_ERROR("asset path not valid");
    }

    config.asset_path = asset_path;

    const auto &rb = iniparser_getstring(ini, "engine:render_backend", "none");

    if (strcmp(rb, "vulkan") == 0) {
        config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
    } else if (strcmp(rb, "directx12") == 0) {
        config.render_backend = RenderBackend::RENDER_BACKEND_DX12;
    }

    config.offscreen = iniparser_getboolean(ini, "app:offscreen", false);

    LOG_INFO("Engine Config: \nWidth: {}\nHeight: {}\nAsset Path: {}\nRender "
             "Backend: {}",
             config.width, config.height, config.asset_path, rb);

    application->Run(config);

    return 0;
}
