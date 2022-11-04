#pragma once

#include "config.h"

class SceneData {
  public:
    SceneData(SceneManager *scene_manager, Backend::RHI *rhi, Camera *camera) noexcept;
    ~SceneData() noexcept = default;

    Camera *cam;
    struct CameraUb {
        Math::float4x4 vp;
        Math::float3 camera_pos;
        f32 ev100;
    } camera_ub;

    Buffer* camera_buffer;

    u32 light_count{};
    Buffer* light_count_buffer;

    Container::Array<Light *> lights;
    Container::Array<LightParams> lights_param_buffer;
    Buffer* light_buffer;

    SceneManager *m_scene_manager{};
};
