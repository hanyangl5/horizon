/*****************************************************************//**
 * \file   SceneManager.cpp
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#include "SceneManager.h"

namespace Horizon {

SceneManager::SceneManager(ResourceManager *resource_manager,
                           std::pmr::polymorphic_allocator<std::byte> allocator) noexcept
    : resource_manager(resource_manager), scene_meshes(allocator), textuer_upload_desc(allocator),
      material_textures(allocator), vertex_buffers(allocator), index_buffers(allocator), instance_params(allocator),
      material_descs(allocator), mesh_data(allocator), scene_indirect_draw_command1(allocator), lights(allocator),
      lights_param_buffer(allocator) {

}

SceneManager::~SceneManager() noexcept {
}

void SceneManager::AddMesh(Mesh *mesh) {
    scene_meshes.push_back(mesh); 
}

void SceneManager::RemoveMesh(Mesh *mesh) {

}

void SceneManager::AddDecal(Decal *decal) { scene_decals.push_back(decal); }

void SceneManager::RemoveDecal(Decal *decal) {

}

void SceneManager::CreateMeshResources(Backend::RHI *rhi) {
    
    u32 texture_offset = 0;
    u32 material_offset = 0;
    u32 vertex_buffer_offset = 0;
    u32 index_buffer_offset = 0;
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
            DrawIndexedInstancedCommand command{};
            command.index_count = primitive.index_count;
            command.first_index = primitive.index_offset;
            command.vertex_offset = 0;
            command.instance_count = 1;
            command.first_instance = 0;
            scene_indirect_draw_command1.push_back(command);
            instance_params.push_back(InstanceParameters{});
            instance_params.back().material_index = primitive.material_id + material_offset;
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

            auto mat = (node.GetModelMatrix() * mesh->transform);

            for (auto &m : node.mesh_primitives) {
                instance_params[primitive_offset + m].model_matrix = mat;
            }
        }
        primitive_offset += mesh->m_mesh_primitives.size();
    }
    // test
    indirect_draw_command_buffer1 = resource_manager->CreateGpuBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_INDIRECT_BUFFER,
                         ResourceState::RESOURCE_STATE_INDIRECT_ARGUMENT, sizeof(DrawIndexedInstancedCommand) * draw_offset});

    instance_parameter_buffer = resource_manager->CreateGpuBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                         sizeof(InstanceParameters) * instance_params.size()});

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
                              scene_indirect_draw_command1.size() * sizeof(DrawIndexedInstancedCommand));

    commandlist->UpdateBuffer(material_description_buffer, material_descs.data(),
                              material_descs.size() * sizeof(MaterialDesc));

    commandlist->UpdateBuffer(instance_parameter_buffer, instance_params.data(), instance_params.size() * sizeof(InstanceParameters));

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

void SceneManager::CreateDecalResources(Backend::RHI *rhi) {

    u32 texture_offset = 0;
    u32 material_offset = 0;
    decal_draw_count++;
    DrawIndexedInstancedCommand command{};
    command.index_count = 36;
    command.first_index = 0;
    command.first_instance = 0;
    command.vertex_offset = 0;
    command.instance_count = 0;
    
    
    // create decal materials
    for (auto &decal : scene_decals) {

        MaterialDesc desc{};

        for (auto &[type, tex] : decal->decal_material->material_textures) {
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

            decal_material_textures.push_back(resource_manager->CreateGpuTexture(create_info));
            TextureUpdateDesc update_desc{};
            update_desc.texture_data_desc = &tex.texture_data_desc;
            decal_textuer_upload_desc.push_back(update_desc);
            texture_offset++;
        }
        //material_offset++;
        auto &material = *decal->decal_material;
        desc.param_bitmask = material.material_params.param_bitmask;
        desc.blend_state = static_cast<u32>(material.blend_state);
        desc.base_color = material.material_params.base_color_factor;
        desc.emissive = material.material_params.emmissive_factor;
        desc.metallic_roughness =
            Math::float2(material.material_params.metallic_factor, material.material_params.roughness_factor);
        decal_material_descs.push_back(desc);

        command.instance_count++; // each instance an decal (default mesh)
        command.instance_count = 0; // TODO: enable decal
        DecalInstanceParameters instance_param{};
        instance_param.model = decal->transform.GetTransformMatrix();
        auto projector_view = Math::LookAt(Math::float3(0, 0, 0), Math::float3(0, 1, 0), Math::float3(0, 1, 0));
        auto projector_projection = Math::Ortho(2, 2, 0.01, 1.0);


        // decal oritation
        Math::float3 n = Math::Normalize(Math::float3(1, 0, 0));
        Math::float3 up = Math::float3(0, 1, 0);
        Math::float3 u = Math::Normalize(Math::Cross(up, n));
        Math::float3 v = Math::Normalize(Math::Cross(n, u));

        Math::float4x4 world_to_decal = Math::float4x4{u, v, n};
        world_to_decal = world_to_decal.Transpose();
        auto &decal_position = decal->transform.GetTranslation();
        world_to_decal._14 = decal_position.Dot(u);
        world_to_decal._24 = decal_position.Dot(v);
        world_to_decal._34 = decal_position.Dot(n);
        instance_param.decal_to_world = world_to_decal.Invert();
        instance_param.world_to_decal = world_to_decal;
        instance_param.material_index = material_offset;
        decal_instance_params.push_back(std::move(instance_param));
    }

    decal_indirect_draw_command.push_back(command);

    decal_material_description_buffer = resource_manager->CreateGpuBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                         sizeof(MaterialDesc) * decal_material_descs.size()});
    decal_indirect_draw_command_buffer1 = resource_manager->CreateGpuBuffer(BufferCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_INDIRECT_BUFFER, ResourceState::RESOURCE_STATE_INDIRECT_ARGUMENT,
        sizeof(DrawIndexedInstancedCommand) * decal_indirect_draw_command.size()});

    decal_instance_parameter_buffer = resource_manager->CreateGpuBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                         sizeof(DecalInstanceParameters) * decal_instance_params.size()});
}

void SceneManager::UploadDecalResources(Backend::CommandList *commandlist) {

    commandlist->UpdateBuffer(decal_material_description_buffer, decal_material_descs.data(),
                              decal_material_descs.size() * sizeof(MaterialDesc));

    commandlist->UpdateBuffer(decal_instance_parameter_buffer, decal_instance_params.data(),
                              decal_instance_params.size() * sizeof(DecalInstanceParameters));

    commandlist->UpdateBuffer(decal_indirect_draw_command_buffer1, decal_indirect_draw_command.data(),
                              decal_indirect_draw_command.size() * sizeof(DrawIndexedInstancedCommand));

    // UPLOAD TEXTURES
    BarrierDesc resource_upload_barrier{};
    TextureBarrierDesc tex_barrier{};
    tex_barrier.src_state = RESOURCE_STATE_COPY_DEST;
    tex_barrier.dst_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
    for (u32 tex = 0; tex < decal_material_textures.size(); tex++) {
        commandlist->UpdateTexture(decal_material_textures[tex], decal_textuer_upload_desc[tex]);
        tex_barrier.texture = decal_material_textures[tex];
        resource_upload_barrier.texture_memory_barriers.push_back(tex_barrier);
    }

    // commandlist->InsertBarrier(resource_upload_barrier);

    BarrierDesc mip_barrier1{};
    for (u32 tex = 0; tex < decal_material_textures.size(); tex++) {
        TextureBarrierDesc mip_map_barrier{};
        mip_map_barrier.texture = decal_material_textures[tex];
        mip_map_barrier.first_mip_level = 0;
        mip_map_barrier.mip_level_count = 1;
        mip_map_barrier.src_state = ResourceState::RESOURCE_STATE_COPY_DEST;
        mip_map_barrier.dst_state = ResourceState::RESOURCE_STATE_COPY_SOURCE;
        mip_barrier1.texture_memory_barriers.emplace_back(mip_map_barrier);
    }
    commandlist->InsertBarrier(mip_barrier1);

    for (u32 tex = 0; tex < decal_material_textures.size(); tex++) {
        commandlist->GenerateMipMap(decal_material_textures[tex], true);
    }

    BarrierDesc mip_barrier2{};
    for (u32 tex = 0; tex < decal_material_textures.size(); tex++) {
        TextureBarrierDesc mip_map_barrier{};
        mip_map_barrier.texture = decal_material_textures[tex];
        mip_map_barrier.first_mip_level = 0;
        mip_map_barrier.mip_level_count = decal_material_textures[tex]->mip_map_level;
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
 Light *SceneManager::AddDirectionalLight(const Math::float3& color, f32 intensity,
                                          const Math::float3& direction) noexcept {
    Light *light = Memory::Alloc<DirectionalLight>(color, intensity, direction);
    lights.emplace_back(light);
    light_count++;
    return light;
 }

 Light *SceneManager::AddPointLight(const Math::float3& color, f32 intensity,
                                    const Math::float3& position, f32 radius) noexcept {
     Light *light = Memory::Alloc<PointLight>(color, intensity, position, radius);
     lights.emplace_back(light);
     light_count++;
     return light;
 }

 Light *SceneManager::AddSpotLight(const Math::float3 &color, f32 intensity, const Math::float3 &position,
                                   const Math::float3 &direction, float radius, f32 inner_cone,
                                   f32 outer_cone) noexcept {
     Light *light = Memory::Alloc<SpotLight>(color, intensity, position, direction, radius, inner_cone, outer_cone);
     lights.emplace_back(light);
     light_count++;
     return light;
 }

std::tuple<Camera*, CameraController*> SceneManager::AddCamera(const CameraSetting &setting, const Math::float3 &position, const Math::float3 &at,
                                 const Math::float3 &up) {
     if (main_camera != nullptr) {
         LOG_WARN("multi veiw is not supported yet");
     }
     main_camera = Memory::MakeUnique<Camera>(setting, position, at, up);
     if (setting.moveable) {
         camera_controller = Memory::MakeUnique<CameraController>(main_camera.get());
     }
     return {main_camera.get(), camera_controller.get()};
 }

 Buffer* SceneManager::GetCameraBuffer() const noexcept { 
     return camera_buffer;
 }

 void SceneManager::CreateLightResources(Backend::RHI *rhi) {
     for (auto &l : lights) {
         lights_param_buffer.push_back(l->GetParamBuffer());
     }
     light_count = lights.size();

     light_count_buffer =
         resource_manager->CreateGpuBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                            ResourceState::RESOURCE_STATE_SHADER_RESOURCE, 4 * sizeof(u32)});

     light_buffer = resource_manager->CreateGpuBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                                       ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                                                       sizeof(LightParams) * light_count});
 }

 void SceneManager::UploadLightResources(Backend::CommandList *commandlist) {
     commandlist->UpdateBuffer(light_buffer, lights_param_buffer.data(),
                               lights_param_buffer.size() * sizeof(LightParams));
     commandlist->UpdateBuffer(light_count_buffer, &light_count, sizeof(light_count) * 4);

 }

 Buffer *SceneManager::GetLightCountBuffer() const noexcept {
    return light_count_buffer;
 }

 Buffer *SceneManager::GetLightParamBuffer() const noexcept { 
     return light_buffer;
 }
 
 void SceneManager::CreateCameraResources(Backend::RHI *rhi) {
     camera_buffer = resource_manager->CreateGpuBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                                                        ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                                                                        sizeof(CameraUb)});
 }

 void SceneManager::UploadCameraResources(Backend::CommandList *commandlist) {
      commandlist->UpdateBuffer(camera_buffer, &camera_ub, sizeof(CameraUb));
 }

 void SceneManager::CreateBuiltInResources(Backend::RHI *rhi) {
     BufferCreateInfo vertex_buffer_create_info{};
     vertex_buffer_create_info.size = cube_vertices.size() * sizeof(Vertex);
     vertex_buffer_create_info.descriptor_types = DescriptorType::DESCRIPTOR_TYPE_VERTEX_BUFFER;
     vertex_buffer_create_info.initial_state = ResourceState::RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
     const_cast<Buffer *>(cube_vertex_buffer) =
         resource_manager->CreateGpuBuffer(vertex_buffer_create_info, "__unit_cube_vb__");
     BufferCreateInfo index_buffer_create_info{};
     index_buffer_create_info.size = cube_indices.size() * sizeof(Index);
     index_buffer_create_info.descriptor_types = DescriptorType::DESCRIPTOR_TYPE_INDEX_BUFFER;
     index_buffer_create_info.initial_state = ResourceState::RESOURCE_STATE_INDEX_BUFFER;
     const_cast<Buffer *>(cube_index_buffer) =
         resource_manager->CreateGpuBuffer(index_buffer_create_info, "__unit_cube_ib__");
 }

 void SceneManager::UploadBuiltInResources(Backend::CommandList *commandlist) {
     if (cube_vertex_buffer) {
         commandlist->UpdateBuffer(cube_vertex_buffer, cube_vertices.data(), sizeof(Vertex) * cube_vertices.size());
     }
     if (cube_index_buffer) {
         commandlist->UpdateBuffer(cube_index_buffer, cube_indices.data(), sizeof(Index) * cube_indices.size());
     }
 }

 Buffer *SceneManager::GetUnitCubeVertexBuffer() const noexcept {
     return cube_vertex_buffer;
 }

 Buffer *SceneManager::GetUnitCubeIndexBuffer() const noexcept {
     return cube_index_buffer;
 }
 } // namespace Horizon