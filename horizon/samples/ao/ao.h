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

class AO {
  public:
    AO() {
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
    
    void InitPipelineResources(); // create pass related resource, shader, pipeline, buffer/tex/rt

    void InitSceneResources(); // create scene related resrouces, mesh, light

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

    Shader *ao_cs;

    Pipeline *ssao_pass;

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

    std::vector<Mesh *> meshes;

    Camera *cam;
    struct CameraUb {
        Math::float4x4 vp;
        Math::float3 camera_pos;
        f32 ev100;
    } camera_ub;

    struct DeferredShadingConstants {
        Math::float4x4 inverse_vp;
        Math::float4 camera_pos;
        u32 width;
        u32 height;
        u32 pad0, pad1;
    } deferred_shading_constants;

    static constexpr u32 SSAO_KERNEL_SIZE = 64;

    struct SSAOConstant {
        Math::float4x4 p;
        Math::float4x4 inv_p;
        u32 width;
        u32 height;
        u32 noise_tex_width = SSAO_NOISE_TEX_WIDTH, noise_tex_height = SSAO_NOISE_TEX_HEIGHT;
        std::array<Math::float4, 64> kernels;
    } ssao_constansts;

    struct ExposureConstant {
        Math::float4 exposure_ev100__;
    } exposure_constants;

    Resource<Buffer> camera_buffer;

    u32 light_count = 1;

    Resource<Buffer> light_count_buffer;
    std::vector<Light *> lights;
    std::vector<LightParams> lights_param_buffer;
    Resource<Buffer> light_buffer;
    Resource<Buffer> deferred_shading_constants_buffer;
    Resource<Buffer> ssao_constants_buffer;
    Resource<Buffer> exposure_constants_buffer;

    static constexpr u32 SSAO_NOISE_TEX_WIDTH = 4;
    static constexpr u32 SSAO_NOISE_TEX_HEIGHT = 4;
    std::array<Math::float2, SSAO_NOISE_TEX_WIDTH * SSAO_NOISE_TEX_HEIGHT> ssao_noise_tex_val;
    Resource<Texture> ssao_noise_tex;

    Resource<Texture> ao_factor_image;
    Resource<Texture> shading_color_image;
    Resource<Texture> pp_color_image;
    

};

