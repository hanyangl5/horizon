/*****************************************************************//**
 * \file   scene_manager.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#include <filesystem>
#include <tuple>

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>

#include <runtime/function/resource/resource_manager/resource_manager.h>
#include <runtime/function/resource/resources/mesh/Mesh.h>
#include <runtime/function/scene/camera/Camera.h>
#include <runtime/function/scene/camera/camera_controller.h>
#include <runtime/function/scene/light/Light.h>

namespace Horizon {

struct MeshData {
    u32 texture_offset;
    u32 vertex_buffer_offset;
    u32 index_buffer_offset;
    u32 draw_offset;
    u32 draw_count;
};

struct InstanceParameters {
    Math::float4x4 model_matrix;
    u32 material_index;
    u32 pad[3];
};

struct DecalInstanceParameters {
    Math::float4x4 model;
    Math::float4x4 decal_to_world;
    Math::float4x4 world_to_decal;
    u32 material_index;
    u32 pad[3];
};

struct MaterialDesc {
    u32 base_color_texture_index;
    u32 normal_texture_index;
    u32 metallic_roughness_texture_index;
    u32 emissive_textue_index;
    u32 alpha_mask_texture_index;
    u32 subsurface_scattering_texture_index;
    u32 param_bitmask;
    u32 blend_state;
    Math::float3 base_color;
    f32 pad1;
    Math::float3 emissive;
    f32 pad2;
    Math::float2 metallic_roughness;
    Math::float2 pad3;
};

class SceneManager {
  public:
    SceneManager(ResourceManager *resource_manager, std::pmr::polymorphic_allocator<std::byte> allocator = {}) noexcept;
    ~SceneManager() noexcept;

    // mesh
    void AddMesh(Mesh *mesh);
    void RemoveMesh(Mesh *mesh);
    void AddDecal(Decal *decal);
    void RemoveDecal(Decal *decal);
    void CreateMeshResources(Backend::RHI *rhi);
    void UploadMeshResources(Backend::CommandList *commandlist);

    void CreateDecalResources(Backend::RHI *rhi);
    void UploadDecalResources(Backend::CommandList *commandlist);
    // light
    Light *AddDirectionalLight(const Math::float3 &color, f32 intensity,
                               const Math::float3 &directiona) noexcept; // temperature, soource radius, length
    Light *AddPointLight(const Math::float3 &color, f32 intensity, const Math::float3 &position, f32 radius) noexcept;
    Light *AddSpotLight(const Math::float3 &color, f32 intensity, const Math::float3 &position,
                        const Math::float3 &direction, f32 radius, f32 inner_cone, f32 outer_cone) noexcept;
    void CreateLightResources(Backend::RHI *rhi);
    void UploadLightResources(Backend::CommandList *commandlist);
    Buffer *GetLightCountBuffer() const noexcept;
    Buffer *GetDirectionalLightParamBuffer() const noexcept;
    Buffer *GetLocalLightParamBuffer() const noexcept;

    Buffer *GetUnitCubeVertexBuffer() const noexcept;
    Buffer *GetUnitCubeIndexBuffer() const noexcept;

    // camera

    Buffer* GetCameraBuffer() const noexcept;
    // TODO(hylu): multiview
    std::tuple<Camera *, CameraController *> AddCamera(const CameraSetting &setting, const Math::float3 &position,
                                                       const Math::float3 &at,
                      const Math::float3 &up);
    void CreateCameraResources(Backend::RHI *rhi);
    void UploadCameraResources(Backend::CommandList *commandlist);

    // miscs
    void GetVertexBuffers(Container::Array<Buffer *> &vertex_buffers, Container::Array<u32> &offsets);

    void CreateBuiltInResources(Backend::RHI *rhi);
    void UploadBuiltInResources(Backend::CommandList* commandlist);

  public:
    // built-in resources

    Buffer *cube_vertex_buffer{};
    Buffer *cube_index_buffer{};

  public:
    ResourceManager *resource_manager{};

    Container::Array<Mesh *> scene_meshes{};
    Container::Array<Decal *> scene_decals{};

    Container::Array<TextureUpdateDesc> textuer_upload_desc{};
    Container::Array<Backend::Texture *> material_textures{};
    Container::Array<Buffer *> vertex_buffers{};
    Container::Array<Buffer *> index_buffers{};

    Container::Array<InstanceParameters> instance_params{};
    Container::Array<MaterialDesc> material_descs{};
    Container::Array<MeshData> mesh_data;
    Buffer *instance_parameter_buffer{};
    Buffer *material_description_buffer{};

    u32 draw_count{0};
    Buffer *indirect_draw_command_buffer1{};
    Container::Array<DrawIndexedInstancedCommand> scene_indirect_draw_command1{};
    Buffer *empty_vertex_buffer{};

    // decal resources

    Container::Array<TextureUpdateDesc> decal_textuer_upload_desc{};
    Container::Array<Backend::Texture *> decal_material_textures{};

    u32 decal_draw_count{0};
    Container::Array<DecalInstanceParameters> decal_instance_params{};

    Container::Array<MaterialDesc> decal_material_descs{};

    Container::Array<DrawIndexedInstancedCommand> decal_indirect_draw_command{};

    Buffer *decal_indirect_draw_command_buffer1{};
    Buffer *decal_instance_parameter_buffer{};
    Buffer *decal_material_description_buffer{};
    // camera

    Memory::UniquePtr<Camera> main_camera{};
    struct CameraUb {
        Math::float4x4 vp;
        Math::float4x4 prev_vp;
        Math::float3 camera_pos;
        f32 ev100;
    }camera_ub{};

    Buffer *camera_buffer{};

    Memory::UniquePtr<CameraController> camera_controller{};

    // light

    struct LightCount {
        u32 directional_light_count{};
        u32 local_light_count{};
    } light_count;

    Buffer *light_count_buffer{};
    Container::Array<Light *> directional_lights{};
    Container::Array<Light *> local_lights{};
    Container::Array<LightParams> directional_lights_params{};
    Container::Array<LightParams> local_lights_params{};
    Buffer *directional_light_buffer{};
    Buffer *local_light_buffer{};

    // scene constants

    Buffer *scene_constants_buffer;
    struct SceneConstants {
        Math::float4x4 camera_view;
        Math::float4x4 camera_projection;
        Math::float4x4 camera_view_projection;
        Math::float4x4 camera_inverse_view_projection;
        u32 resolution[2];
        u32 pad0[2];
        Math::float3 camera_pos;
        u32 pad1;
        f32 ibl_intensity;
    } scene_constants;

    // shadow resources

    struct ShadowMapData {
        Math::float4x4 view_projection;
    };
    u32 shadow_map_count;
    Container::Array<ShadowMapData> shadow_map_data;
};

} // namespace Horizon