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

#include <runtime/core/utils/definations.h>
#include <runtime/core/math/Math.h>
#include <runtime/core/log/Log.h>
#include <runtime/core/utils/renderdoc/RenderDoc.h>
#include <runtime/core/window/Window.h>
#include <runtime/core/units/Units.h>

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
    
    void run();

  private:
    std::unique_ptr<Engine> engine{};
    std::filesystem::path asset_path = "C:/FILES/horizon/horizon/assets";
    u32 width, height;

    Resource<Camera> m_camera{};
    Horizon::RHI::RHI *rhi;
    Resource<SwapChain> swap_chain;

    // pass resources

    Shader *geometry_vs, *geometry_ps;
    Pipeline *geometry_pass;

    Shader *shading_cs;
    Pipeline *shading_pass;

    Resource<RenderTarget> gbuffer0;
    Resource<RenderTarget> gbuffer1;
    Resource<RenderTarget> gbuffer2;
    Resource<RenderTarget> gbuffer3;

    Resource<RenderTarget> depth;

    Resource<Sampler> sampler;

    GraphicsPipelineCreateInfo graphics_pass_ci{};

    Shader *post_process_cs;
    Pipeline *post_process_pass;
    Resource<Texture> shading_color_image;
    Resource<Texture> pp_color_image;

    std::vector<Mesh *> meshes;

    Camera *cam;
    struct CameraUb {
        Math::float4x4 vp;
        Math::float3 camera_pos;
        f32 exposure;
    } camera_ub;

    struct DeferredShadingConstants {
        Math::float4x4 inverse_vp;
        Math::float4 camera_pos;
        u32 width;
        u32 height;
        u32 pad0, pad1;
    } deferred_shading_constants;

    Resource<Buffer> camera_buffer;

    u32 light_count = 1;

    Resource<Buffer> light_count_buffer;
    //Light *directional_light;
    std::vector<Light *> lights;
    std::vector<LightParams> lights_param_buffer;
    Resource<Buffer> light_buffer;
    Resource<Buffer> deferred_shading_constants_buffer;
    bool resources_uploaded = false;

    std::filesystem::path ibl_iem_path;
    std::filesystem::path ibl_pfem_path;
    std::filesystem::path ibl_brdf_lut_path;

    Resource<Texture> ibl_iem, ibl_pfem, ibl_brdf_lut;
    u32 culled_mesh{};
    u32 total_mesh{};
};

