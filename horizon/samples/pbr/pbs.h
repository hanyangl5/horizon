#pragma once
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_set>

#include <iniparser.h>

#include <argparse/argparse.hpp>

#include <runtime/core/log/Log.h>
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

class Pbr {
  public:
    Pbr() {
        EngineConfig config{};
        config.width = 800;
        config.height = 600;
        config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
        config.offscreen = false;
        engine = std::make_unique<Engine>(config);

        width = config.width;
        height = config.height;
    }

    void Init();

    void run();

  private:
    std::unique_ptr<Engine> engine{};
    std::string asset_path = "C:/FILES/horizon/horizon/assets/";
    u32 width, height;

    Resource<Camera> m_camera{};
    Horizon::RHI::RHI *rhi;
    Resource<SwapChain> swap_chain;
    std::string vs_path;
    std::string ps_path;
    Shader *vs, *ps;
    RenderTarget *rt0;
    Resource<RenderTarget> depth;

    Pipeline *pipeline;

    Mesh *mesh;

    Resource<Sampler> sampler;

    Camera *cam;
    GraphicsPipelineCreateInfo info{};
    struct CameraUb {
        Math::float4x4 vp;
        Math::float3 camera_pos;
    } camera_ub;

    Resource<Buffer> camera_buffer;

    u32 light_count = 1;

    Resource<Buffer> light_count_buffer;
    Light *directional_light;

    Resource<Buffer> light_buffer;
    bool resources_uploaded = false;
};
