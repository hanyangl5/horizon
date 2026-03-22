#include "mesh_importer.h"

#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <core/log.h>

#include <resource/resource_loader/texture/texture_loader.h>

#include "mesh.h"
#include "mesh_import_utils.h"

namespace Horizon
{

namespace
{
using namespace MeshImportUtils;

const cgltf_attribute *FindAttribute(const cgltf_primitive &primitive, cgltf_attribute_type type, cgltf_int index = 0)
{
    for (cgltf_size attribute_index = 0; attribute_index < primitive.attributes_count; ++attribute_index)
    {
        const cgltf_attribute &attribute = primitive.attributes[attribute_index];
        if (attribute.type == type && attribute.index == index)
        {
            return &attribute;
        }
    }

    return nullptr;
}

Path ResolveTexturePath(const Path &asset_path, const char *uri)
{
    Path base = asset_path.parent_path();
    base /= uri;
    return base;
}

void AssignTexture(Material &material, MaterialTextureType type, const cgltf_texture_view &view, const Path &asset_path,
                   u32 bitmask)
{
    if (!view.texture)
    {
        return;
    }

    const cgltf_image *image = view.texture->image ? view.texture->image : view.texture->basisu_image;
    if (!image)
    {
        return;
    }

    MaterialTextureDescription description{};
    if (image->uri && std::strncmp(image->uri, "data:", 5) != 0)
    {
        description.url = ResolveTexturePath(asset_path, image->uri);
    }
    else if (image->buffer_view)
    {
        const cgltf_buffer_view *buffer_view = image->buffer_view;
        const u8 *bytes = nullptr;
        if (buffer_view->data)
        {
            bytes = reinterpret_cast<const u8 *>(buffer_view->data);
        }
        else if (buffer_view->buffer && buffer_view->buffer->data)
        {
            bytes = reinterpret_cast<const u8 *>(buffer_view->buffer->data) + buffer_view->offset;
        }

        if (bytes)
        {
            description.texture_data_desc = TextureLoader::LoadFromMemory(bytes, static_cast<u64>(buffer_view->size));
        }
        else
        {
            LOG_WARN("embedded image payload is not available for '{}'", asset_path.string());
        }
    }
    else
    {
        LOG_WARN("unsupported texture source in '{}'", asset_path.string());
        return;
    }

    material.material_textures[type] = std::move(description);
    material.material_params.param_bitmask |= bitmask;
}

void PopulateMaterials(Mesh &mesh, const cgltf_data &data,
                       std::unordered_map<const cgltf_material *, u32> &material_indices)
{
    mesh.materials.resize(std::max<cgltf_size>(1, data.materials_count));

    for (cgltf_size material_index = 0; material_index < data.materials_count; ++material_index)
    {
        const cgltf_material &source = data.materials[material_index];
        Material &destination = mesh.materials[material_index];
        material_indices[&source] = static_cast<u32>(material_index);

        destination.material_params.base_color_factor =
            Math::float3(source.pbr_metallic_roughness.base_color_factor[0],
                         source.pbr_metallic_roughness.base_color_factor[1],
                         source.pbr_metallic_roughness.base_color_factor[2]);
        destination.material_params.metallic_factor = source.pbr_metallic_roughness.metallic_factor;
        destination.material_params.roughness_factor = source.pbr_metallic_roughness.roughness_factor;
        destination.material_params.emmissive_factor =
            Math::float3(source.emissive_factor[0], source.emissive_factor[1], source.emissive_factor[2]);

        if (source.unlit)
        {
            destination.shading_model = ShadingModel::SHADING_MODEL_UNLIT;
        }
        else if (source.double_sided)
        {
            destination.shading_model = ShadingModel::SHADING_MODEL_TWO_SIDE;
        }

        switch (source.alpha_mode)
        {
        case cgltf_alpha_mode_blend:
            destination.blend_state = BlendState::BLEND_STATE_TRANSPARENT;
            break;
        case cgltf_alpha_mode_mask:
            destination.blend_state = BlendState::BLEND_STATE_MASKED;
            destination.material_params.param_bitmask |= HAS_ALPHA;
            break;
        case cgltf_alpha_mode_opaque:
        default:
            destination.blend_state = BlendState::BLEND_STATE_OPAQUE;
            break;
        }

        AssignTexture(destination, MaterialTextureType::BASE_COLOR,
                      source.pbr_metallic_roughness.base_color_texture, mesh.m_asset_path, HAS_BASE_COLOR);
        AssignTexture(destination, MaterialTextureType::NORMAL, source.normal_texture, mesh.m_asset_path, HAS_NORMAL);
        AssignTexture(destination, MaterialTextureType::METALLIC_ROUGHTNESS,
                      source.pbr_metallic_roughness.metallic_roughness_texture, mesh.m_asset_path,
                      HAS_METALLIC_ROUGHNESS);
        AssignTexture(destination, MaterialTextureType::EMISSIVE, source.emissive_texture, mesh.m_asset_path,
                      HAS_EMISSIVE);
    }
}

void PopulateNodes(Mesh &mesh, const cgltf_data &data, std::unordered_map<const cgltf_node *, u32> &node_indices)
{
    mesh.m_nodes.resize(data.nodes_count);

    for (cgltf_size node_index = 0; node_index < data.nodes_count; ++node_index)
    {
        const cgltf_node &source = data.nodes[node_index];
        Node &destination = mesh.m_nodes[node_index];
        node_indices[&source] = static_cast<u32>(node_index);

        destination.name = source.name ? source.name : "";
        destination.parent = INVALID_NODE_INDEX;

        if (source.has_matrix)
        {
            DecomposeExternalMatrix(source.matrix, destination.translation, destination.rotation, destination.scale);
            destination.local_matrix = ExternalToInternalMatrix(source.matrix);
        }
        else
        {
            destination.translation = source.has_translation
                                          ? Math::float3(source.translation[0], source.translation[1], source.translation[2])
                                          : Math::float3(0.0f, 0.0f, 0.0f);
            destination.rotation =
                source.has_rotation ? Math::float4(source.rotation[0], source.rotation[1], source.rotation[2], source.rotation[3])
                                    : Math::float4(0.0f, 0.0f, 0.0f, 1.0f);
            destination.scale =
                source.has_scale ? Math::float3(source.scale[0], source.scale[1], source.scale[2])
                                 : Math::float3(1.0f, 1.0f, 1.0f);
            destination.local_matrix = ComposeMatrix(destination.translation, destination.rotation, destination.scale);
        }
    }

    for (cgltf_size node_index = 0; node_index < data.nodes_count; ++node_index)
    {
        const cgltf_node &source = data.nodes[node_index];
        Node &destination = mesh.m_nodes[node_index];
        if (source.parent)
        {
            destination.parent = node_indices.at(source.parent);
        }
        destination.childs.reserve(source.children_count);
        for (cgltf_size child_index = 0; child_index < source.children_count; ++child_index)
        {
            destination.childs.push_back(node_indices[source.children[child_index]]);
        }
    }
}

void PopulateSkins(Mesh &mesh, const cgltf_data &data, const std::unordered_map<const cgltf_node *, u32> &node_indices,
                   std::unordered_map<const cgltf_skin *, u32> &skin_indices)
{
    mesh.m_skins.resize(data.skins_count);

    for (cgltf_size skin_index = 0; skin_index < data.skins_count; ++skin_index)
    {
        const cgltf_skin &source = data.skins[skin_index];
        MeshSkin &destination = mesh.m_skins[skin_index];
        skin_indices[&source] = static_cast<u32>(skin_index);

        destination.name = source.name ? source.name : "";
        destination.joint_node_indices.resize(source.joints_count, INVALID_NODE_INDEX);
        destination.inverse_bind_matrices.resize(source.joints_count, Math::float4x4::Identity);
        destination.joint_matrices.resize(source.joints_count, Math::float4x4::Identity);

        for (cgltf_size joint_index = 0; joint_index < source.joints_count; ++joint_index)
        {
            const auto found = node_indices.find(source.joints[joint_index]);
            if (found != node_indices.end())
            {
                destination.joint_node_indices[joint_index] = found->second;
            }
        }

        if (source.inverse_bind_matrices)
        {
            const cgltf_size matrix_count = std::min(source.inverse_bind_matrices->count, source.joints_count);
            for (cgltf_size joint_index = 0; joint_index < matrix_count; ++joint_index)
            {
                float matrix_data[16]{};
                if (cgltf_accessor_read_float(source.inverse_bind_matrices, joint_index, matrix_data, 16))
                {
                    destination.inverse_bind_matrices[joint_index] = ExternalToInternalMatrix(matrix_data);
                }
            }
        }
    }
}

bool AppendPrimitive(Mesh &mesh, const cgltf_primitive &primitive, i32 skin_index, u32 material_index)
{
    if (primitive.type != cgltf_primitive_type_triangles)
    {
        LOG_WARN("skipping non-triangle glTF primitive in '{}'", mesh.m_asset_path.string());
        return false;
    }

    const cgltf_attribute *position = FindAttribute(primitive, cgltf_attribute_type_position);
    if (!position || !position->data)
    {
        LOG_WARN("skipping glTF primitive without POSITION in '{}'", mesh.m_asset_path.string());
        return false;
    }

    const cgltf_attribute *normal = FindAttribute(primitive, cgltf_attribute_type_normal);
    const cgltf_attribute *uv0 = FindAttribute(primitive, cgltf_attribute_type_texcoord, 0);
    const cgltf_attribute *uv1 = FindAttribute(primitive, cgltf_attribute_type_texcoord, 1);
    const cgltf_attribute *tangent = FindAttribute(primitive, cgltf_attribute_type_tangent);
    const cgltf_attribute *joints = FindAttribute(primitive, cgltf_attribute_type_joints, 0);
    const cgltf_attribute *weights = FindAttribute(primitive, cgltf_attribute_type_weights, 0);

    const u32 primitive_index = static_cast<u32>(mesh.m_mesh_primitives.size());
    mesh.m_mesh_primitives.emplace_back();

    MeshPrimitive &mesh_primitive = mesh.m_mesh_primitives.back();
    mesh_primitive.index_offset = static_cast<u32>(mesh.m_indices.size());
    mesh_primitive.material_id = material_index;
    mesh_primitive.skin_index = skin_index;

    const u32 vertex_offset = static_cast<u32>(mesh.m_vertices.size());
    const cgltf_size vertex_count = position->data->count;
    mesh.m_vertices.reserve(mesh.m_vertices.size() + vertex_count);

    for (cgltf_size vertex_index = 0; vertex_index < vertex_count; ++vertex_index)
    {
        Vertex vertex{};
        float values[4]{};

        cgltf_accessor_read_float(position->data, vertex_index, values, 3);
        vertex.pos = Math::float3(values[0], values[1], values[2]);

        if ((mesh.vertex_attribute_flag & VertexAttributeType::NORMAL) && normal && normal->data)
        {
            cgltf_accessor_read_float(normal->data, vertex_index, values, 3);
            vertex.normal = Math::float3(values[0], values[1], values[2]);
        }
        if ((mesh.vertex_attribute_flag & VertexAttributeType::UV0) && uv0 && uv0->data)
        {
            cgltf_accessor_read_float(uv0->data, vertex_index, values, 2);
            vertex.uv0 = Math::float2(values[0], 1.0f - values[1]);
        }
        if ((mesh.vertex_attribute_flag & VertexAttributeType::UV1) && uv1 && uv1->data)
        {
            cgltf_accessor_read_float(uv1->data, vertex_index, values, 2);
            vertex.uv1 = Math::float2(values[0], 1.0f - values[1]);
        }
        if ((mesh.vertex_attribute_flag & VertexAttributeType::TANGENT) && tangent && tangent->data)
        {
            cgltf_accessor_read_float(tangent->data, vertex_index, values, 4);
            vertex.tangent = Math::float3(values[0], values[1], values[2]);
        }
        if (joints && joints->data)
        {
            cgltf_accessor_read_float(joints->data, vertex_index, values, 4);
            vertex.joint_indices = Math::float4(values[0], values[1], values[2], values[3]);
        }
        if (weights && weights->data)
        {
            cgltf_accessor_read_float(weights->data, vertex_index, values, 4);
            vertex.joint_weights = Math::float4(values[0], values[1], values[2], values[3]);
            NormalizeJointWeights(vertex.joint_weights);
        }

        mesh.m_vertices.emplace_back(vertex);
    }

    if (primitive.indices)
    {
        mesh_primitive.index_count = static_cast<u32>(primitive.indices->count);
        mesh.m_indices.reserve(mesh.m_indices.size() + primitive.indices->count);
        for (cgltf_size index = 0; index < primitive.indices->count; ++index)
        {
            mesh.m_indices.emplace_back(vertex_offset + static_cast<u32>(cgltf_accessor_read_index(primitive.indices, index)));
        }
    }
    else
    {
        mesh_primitive.index_count = static_cast<u32>(vertex_count);
        mesh.m_indices.reserve(mesh.m_indices.size() + vertex_count);
        for (cgltf_size index = 0; index < vertex_count; ++index)
        {
            mesh.m_indices.emplace_back(vertex_offset + static_cast<u32>(index));
        }
    }

    (void)primitive_index;
    return true;
}

void PopulatePrimitives(Mesh &mesh, const cgltf_data &data,
                        const std::unordered_map<const cgltf_material *, u32> &material_indices,
                        const std::unordered_map<const cgltf_skin *, u32> &skin_indices,
                        const std::unordered_map<const cgltf_node *, u32> &node_indices)
{
    std::unordered_map<const cgltf_mesh *, std::vector<u32>> mesh_primitives_by_mesh;

    for (cgltf_size node_index = 0; node_index < data.nodes_count; ++node_index)
    {
        const cgltf_node &node = data.nodes[node_index];
        if (!node.mesh)
        {
            continue;
        }

        std::vector<u32> &primitive_indices = mesh_primitives_by_mesh[node.mesh];
        if (primitive_indices.empty())
        {
            const i32 skin_index = node.skin ? static_cast<i32>(skin_indices.at(node.skin)) : -1;
            for (cgltf_size primitive_index = 0; primitive_index < node.mesh->primitives_count; ++primitive_index)
            {
                const cgltf_primitive &primitive = node.mesh->primitives[primitive_index];
                u32 material_index = 0;
                if (primitive.material)
                {
                    const auto material_it = material_indices.find(primitive.material);
                    if (material_it != material_indices.end())
                    {
                        material_index = material_it->second;
                    }
                }
                const size_t previous_count = mesh.m_mesh_primitives.size();
                if (AppendPrimitive(mesh, primitive, skin_index, material_index))
                {
                    primitive_indices.push_back(static_cast<u32>(previous_count));
                }
            }
        }

        Node &destination_node = mesh.m_nodes[node_indices.at(&node)];
        destination_node.mesh_primitives = primitive_indices;
        for (u32 primitive_index : primitive_indices)
        {
            if (primitive_index >= mesh.m_mesh_primitives.size())
            {
                continue;
            }
            MeshPrimitive &mesh_primitive = mesh.m_mesh_primitives[primitive_index];
            if (mesh_primitive.node_index == INVALID_NODE_INDEX)
            {
                mesh_primitive.node_index = node_indices.at(&node);
            }
            if (node.skin)
            {
                const auto skin_it = skin_indices.find(node.skin);
                if (skin_it != skin_indices.end())
                {
                    mesh_primitive.skin_index = static_cast<i32>(skin_it->second);
                }
            }
        }
    }
}

void PopulateAnimations(Mesh &mesh, const cgltf_data &data, const std::unordered_map<const cgltf_node *, u32> &node_indices)
{
    mesh.m_animations.reserve(data.animations_count);

    for (cgltf_size animation_index = 0; animation_index < data.animations_count; ++animation_index)
    {
        const cgltf_animation &source = data.animations[animation_index];
        MeshAnimationClip clip{};
        clip.name = source.name ? source.name : "";
        clip.ticks_per_second = 1.0f;
        clip.current_time = 0.0f;

        std::unordered_map<u32, size_t> channel_indices;

        for (cgltf_size channel_index = 0; channel_index < source.channels_count; ++channel_index)
        {
            const cgltf_animation_channel &channel = source.channels[channel_index];
            if (!channel.target_node || !channel.sampler || !channel.sampler->input || !channel.sampler->output)
            {
                continue;
            }

            const auto node_it = node_indices.find(channel.target_node);
            if (node_it == node_indices.end())
            {
                continue;
            }

            size_t destination_channel_index = 0;
            const auto existing_channel = channel_indices.find(node_it->second);
            if (existing_channel == channel_indices.end())
            {
                destination_channel_index = clip.channels.size();
                channel_indices[node_it->second] = destination_channel_index;
                clip.channels.emplace_back();
                clip.channels.back().node_index = node_it->second;
            }
            else
            {
                destination_channel_index = existing_channel->second;
            }

            MeshNodeAnimationChannel &destination_channel = clip.channels[destination_channel_index];
            const cgltf_accessor *input = channel.sampler->input;
            const cgltf_accessor *output = channel.sampler->output;
            const cgltf_size key_count = std::min(input->count, output->count);

            switch (channel.target_path)
            {
            case cgltf_animation_path_type_translation:
                destination_channel.position_times.reserve(key_count);
                destination_channel.position_values.reserve(key_count);
                for (cgltf_size key_index = 0; key_index < key_count; ++key_index)
                {
                    float time_value[1]{};
                    float vector_value[3]{};
                    cgltf_accessor_read_float(input, key_index, time_value, 1);
                    cgltf_accessor_read_float(output, key_index, vector_value, 3);
                    destination_channel.position_times.push_back(time_value[0]);
                    destination_channel.position_values.emplace_back(vector_value[0], vector_value[1], vector_value[2]);
                    clip.duration = std::max(clip.duration, time_value[0]);
                }
                break;
            case cgltf_animation_path_type_rotation:
                destination_channel.rotation_times.reserve(key_count);
                destination_channel.rotation_values.reserve(key_count);
                for (cgltf_size key_index = 0; key_index < key_count; ++key_index)
                {
                    float time_value[1]{};
                    float quat_value[4]{};
                    cgltf_accessor_read_float(input, key_index, time_value, 1);
                    cgltf_accessor_read_float(output, key_index, quat_value, 4);
                    destination_channel.rotation_times.push_back(time_value[0]);
                    destination_channel.rotation_values.emplace_back(quat_value[0], quat_value[1], quat_value[2],
                                                                     quat_value[3]);
                    clip.duration = std::max(clip.duration, time_value[0]);
                }
                break;
            case cgltf_animation_path_type_scale:
                destination_channel.scale_times.reserve(key_count);
                destination_channel.scale_values.reserve(key_count);
                for (cgltf_size key_index = 0; key_index < key_count; ++key_index)
                {
                    float time_value[1]{};
                    float vector_value[3]{};
                    cgltf_accessor_read_float(input, key_index, time_value, 1);
                    cgltf_accessor_read_float(output, key_index, vector_value, 3);
                    destination_channel.scale_times.push_back(time_value[0]);
                    destination_channel.scale_values.emplace_back(vector_value[0], vector_value[1], vector_value[2]);
                    clip.duration = std::max(clip.duration, time_value[0]);
                }
                break;
            default:
                break;
            }
        }

        if (!clip.channels.empty() && clip.duration > 0.0f)
        {
            mesh.m_animations.emplace_back(std::move(clip));
        }
    }
}

} // namespace

bool LoadMeshWithCgltf(Mesh &mesh)
{
    cgltf_options options{};
    cgltf_data *data = nullptr;

    const std::string asset_path = mesh.m_asset_path.string();
    cgltf_result result = cgltf_parse_file(&options, asset_path.c_str(), &data);
    if (result != cgltf_result_success || !data)
    {
        LOG_ERROR("failed to parse glTF '{}': {}", asset_path, static_cast<int>(result));
        return false;
    }

    result = cgltf_load_buffers(&options, data, asset_path.c_str());
    if (result != cgltf_result_success)
    {
        LOG_ERROR("failed to load glTF buffers '{}': {}", asset_path, static_cast<int>(result));
        cgltf_free(data);
        return false;
    }

    result = cgltf_validate(data);
    if (result != cgltf_result_success)
    {
        LOG_WARN("glTF validation reported issues for '{}': {}", asset_path, static_cast<int>(result));
    }

    std::unordered_map<const cgltf_node *, u32> node_indices;
    std::unordered_map<const cgltf_material *, u32> material_indices;
    std::unordered_map<const cgltf_skin *, u32> skin_indices;

    PopulateNodes(mesh, *data, node_indices);
    PopulateMaterials(mesh, *data, material_indices);
    PopulateSkins(mesh, *data, node_indices, skin_indices);
    PopulatePrimitives(mesh, *data, material_indices, skin_indices, node_indices);
    PopulateAnimations(mesh, *data, node_indices);

    if (mesh.materials.empty())
    {
        mesh.materials.resize(1);
    }

    cgltf_free(data);
    return !mesh.m_mesh_primitives.empty();
}

} // namespace Horizon
