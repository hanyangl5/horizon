#pragma once
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <filesystem>
#include <unordered_set>
#include <random>
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
        config.width = 1600;
        config.height = 900;
        config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
        config.offscreen = false;
        engine = std::make_unique<Engine>(config);

        width = config.width;
        height = config.height;
    }

    void Init() {
        InitAPI();
        InitResources();
    }
    void InitAPI();

    void InitResources();

    void InitSphere();
    
    void run();

  private:
    std::unique_ptr<Engine> engine{};
    std::filesystem::path asset_path = "C:/FILES/horizon/horizon/assets";
    u32 width, height;

    Resource<Camera> m_camera{};
    Horizon::RHI::RHI *rhi;
    Resource<SwapChain> swap_chain;

    // pass resources

    std::filesystem::path opaque_vs_path, opaque_ps_path;
    Shader *opaque_vs, *opaque_ps;

    RenderTarget *rt0;
    Resource<RenderTarget> depth;
    Resource<Sampler> sampler;

    GraphicsPipelineCreateInfo graphics_pass_ci{};
    Pipeline *opaque_pass;

    std::filesystem::path masked_vs_path, masked_ps_path;
    Shader *masked_vs, *masked_ps;

    Pipeline *masked_pass;


    //

    Mesh *mesh;
    std::vector<Mesh *> meshes;

    Camera *cam;
    struct CameraUb {
        Math::float4x4 vp;
        Math::float3 camera_pos;
        f32 exposure;
    } camera_ub;

    Resource<Buffer> camera_buffer;

    u32 light_count = 1;

    Resource<Buffer> light_count_buffer;
    //Light *directional_light;
    std::vector<Light *> lights;
    std::vector<LightParams> lights_param_buffer;
    Resource<Buffer> light_buffer;
    bool resources_uploaded = false;

    u32 culled_mesh{};
    u32 total_mesh{};
};