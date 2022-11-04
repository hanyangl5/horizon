#pragma once

#include <filesystem>

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>
#include <runtime/function/resource/resource_manager/ResourceManager.h>
#include <runtime/function/resource/resources/mesh/Mesh.h>
#include <runtime/function/scene/camera/Camera.h>
#include <runtime/function/scene/light/Light.h>

namespace Horizon {

struct MeshData {
    u32 texture_offset;
    u32 vertex_buffer_offset;
    u32 index_buffer_offset;
    u32 draw_offset;
    u32 draw_count;
};

struct DrawParameters {
    Math::float4x4 model_matrix;
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
    SceneManager(ResourceManager *resource_manager) noexcept;
    ~SceneManager() noexcept;

    void AddMesh(Mesh *mesh);

    void RemoveMesh(Mesh *mesh);

    void CreateMeshResources(Backend::RHI *rhi);

    void UploadMeshResources(Backend::CommandList *commandlist);
    // Light *AddDirectionalLight(
    //     Math::color color, f32 intensity,
    //     Math::float3
    //         directiona) noexcept; // temperature, soource radius, length
    // Light *AddPointLight(Math::float3 color, f32 intensity,
    //                      f32 radius) noexcept;
    // Light *AddSpotLight(Math::float3 color, f32 intensity, f32 inner_cone,
    //                     f32 outer_cone) noexcept;
    // Camera *SetMainCamera() noexcept;
    void GetVertexBuffers(Container::Array<Buffer *> &vertex_buffers, Container::Array<u32> &offsets);

  public:
    ResourceManager *resource_manager;

    Container::Array<Mesh *> scene_meshes{};
    Container::Array<Light*> lights;
    Container::Array<TextureUpdateDesc> textuer_upload_desc{};
    Container::Array<Backend::Texture *> material_textures;
    Container::Array<Buffer *> vertex_buffers;
    Container::Array<Buffer *> index_buffers;

    Container::Array<DrawParameters> draw_params{};
    Container::Array<MaterialDesc> material_descs{};
    Container::Array<MeshData> mesh_data;
    Buffer *draw_parameter_buffer{};
    Buffer *material_description_buffer{};

    u32 draw_count = 0;
    Buffer *indirect_draw_command_buffer1;
    Container::Array<IndirectDrawCommand> scene_indirect_draw_command1;
    Buffer *empty_vertex_buffer;
};

} // namespace Horizon