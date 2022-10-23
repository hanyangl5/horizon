#pragma once

#include "config.h"

class SceneData {
  public:
    SceneData(Backend::RHI *rhi, Camera *camera) noexcept;
    ~SceneData() noexcept = default;

    std::vector<Mesh *> meshes;

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

    
    // each mesh one draw call
    std::vector<Resource<Buffer>> indirect_draw_command_buffers;
    std::vector<std::vector<IndirectDrawCommand>> scene_indirect_draw_commands;

    u32 draw_count = 0;
    Resource<Buffer> indirect_draw_command_buffer1;
    std::vector<IndirectDrawCommand> scene_indirect_draw_command1;
};
