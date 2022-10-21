
#pragma once
#include <chrono>
#include <filesystem>
#include <iniparser.h>
#include <memory>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <string>
#include <unordered_set>

#include <argparse/argparse.hpp>

#include <runtime/core/log/Log.h>
#include <runtime/core/math/Math.h>
#include <runtime/core/units/Units.h>
#include <runtime/core/utils/definations.h>
#include <runtime/core/utils/renderdoc/RenderDoc.h>
#include <runtime/core/window/Window.h>

#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/scene/geometry/mesh/Mesh.h>
#include <runtime/function/scene/light/Light.h>

#include <runtime/interface/Engine.h>

#include <runtime/system/input/InputSystem.h>
#include <runtime/system/render/RenderSystem.h>

using namespace Horizon;
using namespace Horizon::RHI;

// MipGen, a sample for general compute

class MipmapGen {
  public:
    MipmapGen() {
        EngineConfig config{};
        config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
        config.offscreen = true;
        engine = std::make_unique<Engine>(config);
    }

    void Init() {
        InitAPI();
        InitResources();
    }
    void InitAPI();

    void InitResources();

    void InitPipelineResources(); // create pass related resource, shader, pipeline, buffer/tex/rt

    void InitSceneResources(); // create scene related resrouces, mesh, light

    void run();

  private:
    std::unique_ptr<Engine> engine{};
    std::filesystem::path asset_path = "C:/FILES/horizon/horizon/assets";

    struct TextureData {
        std::filesystem::path path;
        void *data;
        u32 width, height;
        DescriptorSet *ds;
        Resource<Texture> texture;
    };

    std::vector<TextureData> texture_resources{};

    Horizon::RHI::RHI *rhi;


    Shader *mipgen_cs;
    Pipeline *mipgen_pass;

};

// HBAO

// HDAO

// GTAO