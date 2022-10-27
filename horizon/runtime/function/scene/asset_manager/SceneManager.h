#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>
#include <runtime/function/scene/camera/Camera.h>
#include <runtime/function/scene/geometry/mesh/Mesh.h>
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
    u32 pad0;
    Math::float3 base_color;
    f32 pad1;
    Math::float3 emissive;
    f32 pad2;
    Math::float2 metallic_roughness;
    Math::float2 pad3;
};
class SceneManager {
  public:
    SceneManager() noexcept;
    ~SceneManager() noexcept;

    Mesh *AddMesh(const MeshDesc &desc, const std::filesystem::path &path) noexcept;

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
    void GetVertexBuffers(std::vector<Backend::Buffer *> &vertex_buffers, std::vector<u32> &offsets);

  public:
    std::vector<Resource<Mesh>> meshes;
    std::vector<std::unique_ptr<Light>> lights;
    std::vector<TextureUpdateDesc> textuer_upload_desc{};
    std::vector<Resource<Backend::Texture>> textures;
    std::vector<Resource<Backend::Buffer>> vertex_buffers;
    std::vector<Resource<Backend::Buffer>> index_buffers;

    std::vector<DrawParameters> draw_params{};
    std::vector<MaterialDesc> material_descs{};
    std::vector<MeshData> mesh_data;
    Resource<Backend::Buffer> draw_parameter_buffer{};
    Resource<Backend::Buffer> material_description_buffer{};

        
    // each mesh one draw call
    //std::vector<Resource<Backend::Buffer>> indirect_draw_command_buffers;
    //std::vector<std::vector<IndirectDrawCommand>> scene_indirect_draw_commands;

    u32 draw_count = 0;
    Resource<Backend::Buffer> indirect_draw_command_buffer1;
    std::vector<IndirectDrawCommand> scene_indirect_draw_command1;
    // std::unique_ptr<SceneManager> scene_manager;


    // std::vector<Buffer *> buffers;
};

} // namespace Horizon