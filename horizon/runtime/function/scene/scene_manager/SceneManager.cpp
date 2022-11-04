#include "SceneManager.h"

namespace Horizon {

SceneManager::SceneManager(ResourceManager *resource_manager) noexcept : resource_manager(resource_manager) {}

SceneManager::~SceneManager() noexcept {}

void SceneManager::AddMesh(Mesh *mesh) { scene_meshes.push_back(mesh); }

void SceneManager::RemoveMesh(Mesh *mesh) {}

void SceneManager::CreateMeshResources(Backend::RHI *rhi) {

    u32 texture_offset = 0;
    u32 vertex_buffer_offset = 0;
    u32 index_buffer_offset = 0;
    u32 material_offset = 0;
    u32 draw_offset = 0;
    u32 draw_count = 0;
    for (auto &mesh : scene_meshes) {
        draw_count = mesh->m_mesh_primitives.size();
        mesh_data.push_back(
            MeshData{texture_offset, vertex_buffer_offset, index_buffer_offset, draw_offset, draw_count});
        draw_offset += draw_count;
        BufferCreateInfo vertex_buffer_create_info{};
        vertex_buffer_create_info.size = mesh->m_vertices.size() * sizeof(Vertex);
        vertex_buffer_create_info.descriptor_types = DescriptorType::DESCRIPTOR_TYPE_VERTEX_BUFFER;
        vertex_buffer_create_info.initial_state = ResourceState::RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        vertex_buffers.push_back(resource_manager->CreateGpuBuffer(vertex_buffer_create_info));

        BufferCreateInfo index_buffer_create_info{};
        index_buffer_create_info.size = mesh->m_indices.size() * sizeof(Index);
        index_buffer_create_info.descriptor_types = DescriptorType::DESCRIPTOR_TYPE_INDEX_BUFFER;
        index_buffer_create_info.initial_state = ResourceState::RESOURCE_STATE_INDEX_BUFFER;
        index_buffers.push_back(resource_manager->CreateGpuBuffer(index_buffer_create_info));
        vertex_buffer_offset++;
        index_buffer_offset++;

        // indirect draw command
        for (auto &primitive : mesh->m_mesh_primitives) {
            IndirectDrawCommand command{};
            command.index_count = primitive.index_count;
            command.first_index = primitive.index_offset;
            command.vertex_offset = 0;
            command.instance_count = 1;
            command.first_instance = 0;
            scene_indirect_draw_command1.push_back(command);
            draw_params.push_back(DrawParameters{});
            draw_params.back().material_index = primitive.material_id + material_offset;
        }

        for (auto &material : mesh->materials) {

            MaterialDesc desc{};

            for (auto &[type, tex] : material.material_textures) {
                switch (type) {
                case MaterialTextureType::BASE_COLOR:
                    desc.base_color_texture_index = texture_offset;
                    break;
                case MaterialTextureType::NORMAL:
                    desc.normal_texture_index = texture_offset;
                    break;
                case MaterialTextureType::METALLIC_ROUGHTNESS:
                    desc.metallic_roughness_texture_index = texture_offset;
                    break;
                case MaterialTextureType::EMISSIVE:
                    desc.emissive_textue_index = texture_offset;
                    break;
                case MaterialTextureType::ALPHA_MASK:
                    desc.alpha_mask_texture_index = texture_offset;
                    break;
                default:
                    break;
                }

                TextureCreateInfo create_info{};

                create_info.width = tex.texture_data_desc.width;
                create_info.height = tex.texture_data_desc.height;

                create_info.depth = 1;
                create_info.texture_format = TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM; // TOOD: optimize format
                create_info.texture_type = TextureType::TEXTURE_TYPE_2D;                // TODO: cubemap?
                create_info.descriptor_types = DescriptorType::DESCRIPTOR_TYPE_TEXTURE;
                create_info.initial_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
                create_info.enanble_mipmap = true;

                material_textures.push_back(resource_manager->CreateGpuTexture(create_info));
                TextureUpdateDesc update_desc{};
                update_desc.texture_data_desc = &tex.texture_data_desc;
                textuer_upload_desc.push_back(update_desc);
                texture_offset++;
            }

            material_offset++;
            desc.param_bitmask = material.material_params.param_bitmask;
            desc.blend_state = static_cast<u32>(material.blend_state);
            desc.base_color = material.material_params.base_color_factor;
            desc.emissive = material.material_params.emmissive_factor;
            desc.metallic_roughness =
                Math::float2(material.material_params.metallic_factor, material.material_params.roughness_factor);
            material_descs.push_back(desc);
        }
    }
    material_description_buffer = resource_manager->CreateGpuBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                         sizeof(MaterialDesc) * material_descs.size()});

    // material_offset = 0;
    u32 primitive_offset = 0;
    for (auto &mesh : scene_meshes) {

        for (auto &node : mesh->GetNodes()) {

            auto mat = (node.GetModelMatrix() * mesh->transform).Transpose();

            for (auto &m : node.mesh_primitives) {
                draw_params[primitive_offset + m].model_matrix = mat;
            }
        }
        primitive_offset += mesh->m_mesh_primitives.size();
    }
    // test
    indirect_draw_command_buffer1 = resource_manager->CreateGpuBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_INDIRECT_BUFFER,
                         ResourceState::RESOURCE_STATE_INDIRECT_ARGUMENT, sizeof(IndirectDrawCommand) * draw_offset});

    draw_parameter_buffer = resource_manager->CreateGpuBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                         sizeof(DrawParameters) * draw_params.size()});

    empty_vertex_buffer = resource_manager->GetEmptyVertexBuffer();
}

void SceneManager::UploadMeshResources(Backend::CommandList *commandlist) {

    for (u32 i = 0; i < scene_meshes.size(); i++) {
        auto &mesh = scene_meshes[i];
        // upload vertex and index buffer
        commandlist->UpdateBuffer(vertex_buffers[i], mesh->m_vertices.data(), vertex_buffers[i]->m_size);
        commandlist->UpdateBuffer(index_buffers[i], mesh->m_indices.data(), index_buffers[i]->m_size);
    }

    commandlist->UpdateBuffer(indirect_draw_command_buffer1, scene_indirect_draw_command1.data(),
                              scene_indirect_draw_command1.size() * sizeof(IndirectDrawCommand));

    commandlist->UpdateBuffer(material_description_buffer, material_descs.data(),
                              material_descs.size() * sizeof(MaterialDesc));

    commandlist->UpdateBuffer(draw_parameter_buffer, draw_params.data(), draw_params.size() * sizeof(DrawParameters));

    // UPLOAD TEXTURES
    BarrierDesc resource_upload_barrier{};
    TextureBarrierDesc tex_barrier{};
    tex_barrier.src_state = RESOURCE_STATE_COPY_DEST;
    tex_barrier.dst_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
    for (u32 tex = 0; tex < material_textures.size(); tex++) {
        commandlist->UpdateTexture(material_textures[tex], textuer_upload_desc[tex]);
        tex_barrier.texture = material_textures[tex];
        resource_upload_barrier.texture_memory_barriers.push_back(tex_barrier);
    }

    // commandlist->InsertBarrier(resource_upload_barrier);

    BarrierDesc mip_barrier1{};
    for (u32 tex = 0; tex < material_textures.size(); tex++) {
        TextureBarrierDesc mip_map_barrier{};
        mip_map_barrier.texture = material_textures[tex];
        mip_map_barrier.first_mip_level = 0;
        mip_map_barrier.mip_level_count = 1;
        mip_map_barrier.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
        mip_map_barrier.dst_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
        mip_barrier1.texture_memory_barriers.emplace_back(mip_map_barrier);
    }
    commandlist->InsertBarrier(mip_barrier1);

    for (u32 tex = 0; tex < material_textures.size(); tex++) {
        commandlist->GenerateMipMap(material_textures[tex], true);
    }

    BarrierDesc mip_barrier2{};
    for (u32 tex = 0; tex < material_textures.size(); tex++) {
        TextureBarrierDesc mip_map_barrier{};
        mip_map_barrier.texture = material_textures[tex];
        mip_map_barrier.first_mip_level = 0;
        mip_map_barrier.mip_level_count = material_textures[tex]->mip_map_level;
        mip_map_barrier.src_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
        mip_map_barrier.dst_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
        mip_barrier2.texture_memory_barriers.emplace_back(mip_map_barrier);
    }
    commandlist->InsertBarrier(mip_barrier2);
}

// Mesh *SceneManager::AddMesh(BasicGeometry basic_geometry) noexcept {
//     auto &m = meshes.emplace_back();
//     m->LoadMesh(basic_geometry);
//     return m.get();
// }
//
// Light *SceneManager::AddDirectionalLight(Math::color color, f32 intensity,
//                                          Math::float3 direction) noexcept {
//     auto &l = lights.emplace_back(
//         Memory::Alloc<DirectionalLight>(color, intensity, direction));
//     return l.get();
// }
//
// Light *SceneManager::AddPointLight(Math::float3 color, f32 intensity,
//                                    f32 radius) noexcept {
//     auto &l = lights.emplace_back(
//         Memory::Alloc<PointLight>(color, intensity, radius));
//     return l.get();
// }
//
// Light *SceneManager::AddSpotLight(Math::float3 color, f32 intensity,
//                                   f32 inner_cone, f32 outer_cone) noexcept {
//     auto &l = lights.emplace_back(
//         Memory::Alloc<SpotLight>(color, intensity, inner_cone, outer_cone));
//     return l.get();
// }
} // namespace Horizon