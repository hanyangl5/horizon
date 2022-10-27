#pragma once

#include "config.h"

class SceneData {
  public:
    SceneData(Backend::RHI *rhi, Camera *camera) noexcept;
    ~SceneData() noexcept = default;

    Camera *cam;
    struct CameraUb {
        Math::float4x4 vp;
        Math::float3 camera_pos;
        f32 ev100;
    } camera_ub;

    Resource<Buffer> camera_buffer;

    u32 light_count{};
    Resource<Buffer> light_count_buffer;

    std::vector<Light *> lights;
    std::vector<LightParams> lights_param_buffer;
    Resource<Buffer> light_buffer;

    std::unique_ptr<SceneManager> scene_manager{};

};
