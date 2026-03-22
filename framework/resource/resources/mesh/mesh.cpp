#include "mesh.h"

#include <algorithm>
#include <cmath>
#include <thread>

#include <core/log.h>

#include <resource/resource_loader/texture/texture_loader.h>

#include "mesh_import_utils.h"
#include "mesh_importer.h"

namespace Horizon
{

namespace
{
using namespace MeshImportUtils;

Math::float3 SampleVec3Track(const std::vector<f32> &times, const std::vector<Math::float3> &values, f32 time)
{
    if (times.empty() || values.empty())
    {
        return Math::float3(0.0f, 0.0f, 0.0f);
    }
    if (times.size() == 1 || values.size() == 1 || time <= times.front())
    {
        return values.front();
    }
    if (time >= times.back())
    {
        return values.back();
    }

    for (size_t i = 0; i + 1 < times.size(); ++i)
    {
        if (time >= times[i] && time <= times[i + 1])
        {
            const f32 delta = times[i + 1] - times[i];
            const f32 factor = delta > 0.0f ? (time - times[i]) / delta : 0.0f;
            return values[i] + (values[i + 1] - values[i]) * factor;
        }
    }

    return values.back();
}

Math::float4 SampleQuatTrack(const std::vector<f32> &times, const std::vector<Math::float4> &values, f32 time)
{
    if (times.empty() || values.empty())
    {
        return Math::float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    if (times.size() == 1 || values.size() == 1 || time <= times.front())
    {
        return values.front();
    }
    if (time >= times.back())
    {
        return values.back();
    }

    for (size_t i = 0; i + 1 < times.size(); ++i)
    {
        if (time >= times[i] && time <= times[i + 1])
        {
            const f32 delta = times[i + 1] - times[i];
            const f32 factor = delta > 0.0f ? (time - times[i]) / delta : 0.0f;
            return SlerpQuaternion(values[i], values[i + 1], factor);
        }
    }

    return values.back();
}
} // namespace

Mesh::Mesh(const MeshDesc &desc, const char *path) noexcept
    : vertex_attribute_flag(desc.vertex_attribute_flag), mesh_format(desc.mesh_format), m_asset_path(path)
{
}

Mesh::~Mesh() noexcept = default;

void Mesh::ResetImportData()
{
    m_mesh_primitives.clear();
    m_vertices.clear();
    m_indices.clear();
    m_nodes.clear();
    materials.clear();
    m_skins.clear();
    m_skin_joint_offsets.clear();
    m_joint_matrices.clear();
    m_animations.clear();
    m_node_name_to_index.clear();
    m_active_animation = 0;
}

void Mesh::RebuildNodeNameMap()
{
    m_node_name_to_index.clear();
    for (u32 node_index = 0; node_index < m_nodes.size(); ++node_index)
    {
        if (!m_nodes[node_index].name.empty())
        {
            m_node_name_to_index[m_nodes[node_index].name] = node_index;
        }
    }
}

void Mesh::LoadMaterialTextures()
{
    if (materials.empty())
    {
        return;
    }

    auto &mats = materials;
    std::vector<std::thread> threads;
    const u32 hardware_threads = std::thread::hardware_concurrency();
    const u32 max_available_thread = hardware_threads > 1 ? hardware_threads - 1 : 1;
    threads.reserve(max_available_thread);

    const u32 block_image_size = static_cast<u32>(mats.size()) / max_available_thread + 1;

    for (u32 i = 0; i < max_available_thread; ++i)
    {
        threads.emplace_back([&mats, block_image_size, i]() {
            for (u32 j = i * block_image_size; j < (i + 1) * block_image_size && j < mats.size(); ++j)
            {
                for (auto &[type, tex] : mats[j].material_textures)
                {
                    (void)type;
                    if (!tex.texture_data_desc.raw_data.empty())
                    {
                        continue;
                    }
                    if (tex.url.empty())
                    {
                        continue;
                    }
                    tex.texture_data_desc = TextureLoader::Load(tex.url.c_str());
                }
            }
        });
    }

    for (auto &thread : threads)
    {
        thread.join();
    }
}

void Mesh::UpdateNodeMatrices()
{
    for (u32 i = 0; i < m_nodes.size(); ++i)
    {
        auto &node = m_nodes[i];
        node.local_matrix = MeshImportUtils::ComposeMatrix(node.translation, node.rotation, node.scale);

        if (node.parent == MeshImportUtils::INVALID_NODE_INDEX)
        {
            node.model_matrix = node.local_matrix;
        }
        else
        {
            node.model_matrix = node.local_matrix * m_nodes[node.parent].model_matrix;
        }
    }
}

void Mesh::UpdateSkinMatrices()
{
    for (u32 primitive_index = 0; primitive_index < m_mesh_primitives.size(); ++primitive_index)
    {
        const auto &primitive = m_mesh_primitives[primitive_index];
        if (primitive.skin_index < 0 || static_cast<u32>(primitive.skin_index) >= m_skins.size())
        {
            continue;
        }
        if (primitive.node_index == MeshImportUtils::INVALID_NODE_INDEX || primitive.node_index >= m_nodes.size())
        {
            continue;
        }

        auto &skin = m_skins[static_cast<u32>(primitive.skin_index)];
        const Math::float4x4 inverse_mesh_transform = m_nodes[primitive.node_index].model_matrix.Invert();

        const size_t joint_count = std::min(skin.joint_node_indices.size(), skin.inverse_bind_matrices.size());
        skin.joint_matrices.resize(joint_count);
        for (size_t joint_index = 0; joint_index < joint_count; ++joint_index)
        {
            const u32 node_index = skin.joint_node_indices[joint_index];
            if (node_index == MeshImportUtils::INVALID_NODE_INDEX || node_index >= m_nodes.size())
            {
                skin.joint_matrices[joint_index] = Math::float4x4::Identity;
                continue;
            }

            skin.joint_matrices[joint_index] =
                skin.inverse_bind_matrices[joint_index] * m_nodes[node_index].model_matrix * inverse_mesh_transform;
        }
    }
}

void Mesh::FlattenJointMatrices()
{
    m_skin_joint_offsets.resize(m_skins.size());
    m_joint_matrices.clear();

    for (u32 i = 0; i < m_skins.size(); ++i)
    {
        m_skin_joint_offsets[i] = static_cast<u32>(m_joint_matrices.size());
        auto &skin = m_skins[i];
        m_joint_matrices.insert(m_joint_matrices.end(), skin.joint_matrices.begin(), skin.joint_matrices.end());
    }
}

void Mesh::Load()
{
    if (!m_vertices.empty())
    {
        LOG_ERROR("mesh already loaded");
        return;
    }

    ResetImportData();

    bool loaded = false;
    switch (mesh_format)
    {
    case EMeshAssetFormat::MESH_FORMAT_GLTF:
    case EMeshAssetFormat::MESH_FORMAT_GLB:
        loaded = LoadMeshWithCgltf(*this);
        break;
    case EMeshAssetFormat::MESH_FORMAT_FBX:
        loaded = LoadMeshWithFbxSdk(*this);
        break;
    default:
        LOG_ERROR("{} format is not supportted", static_cast<u32>(mesh_format));
        return;
    }

    if (!loaded)
    {
        ResetImportData();
        return;
    }

    RebuildNodeNameMap();
    UpdateNodeMatrices();
    LoadMaterialTextures();
    UpdateSkinMatrices();
    FlattenJointMatrices();

    m_vertices.shrink_to_fit();
    m_indices.shrink_to_fit();

    LOG_DEBUG("mesh successfully loaded, {} primitives, {} vertices, {} indices, {} skins, {} animations",
              m_mesh_primitives.size(), m_vertices.size(), m_indices.size(), m_skins.size(), m_animations.size());
}

void Mesh::UpdateAnimation(f32 delta_time_seconds)
{
    if (m_animations.empty() || m_active_animation >= m_animations.size())
    {
        return;
    }

    auto &animation = m_animations[m_active_animation];
    if (animation.duration <= 0.0f)
    {
        return;
    }

    animation.current_time += delta_time_seconds * animation.ticks_per_second;
    animation.current_time = std::fmod(animation.current_time, animation.duration);
    if (animation.current_time < 0.0f)
    {
        animation.current_time += animation.duration;
    }

    for (const auto &channel : animation.channels)
    {
        if (channel.node_index >= m_nodes.size())
        {
            continue;
        }

        auto &node = m_nodes[channel.node_index];
        if (!channel.position_times.empty() && !channel.position_values.empty())
        {
            node.translation = SampleVec3Track(channel.position_times, channel.position_values, animation.current_time);
        }
        if (!channel.rotation_times.empty() && !channel.rotation_values.empty())
        {
            node.rotation = SampleQuatTrack(channel.rotation_times, channel.rotation_values, animation.current_time);
        }
        if (!channel.scale_times.empty() && !channel.scale_values.empty())
        {
            node.scale = SampleVec3Track(channel.scale_times, channel.scale_values, animation.current_time);
        }
    }

    UpdateNodeMatrices();
    UpdateSkinMatrices();
    FlattenJointMatrices();
}

const std::vector<Node> &Mesh::GetNodes() const noexcept
{
    return m_nodes;
}

const std::vector<Math::float4x4> &Mesh::GetJointMatrices() const noexcept
{
    return m_joint_matrices;
}

bool Mesh::HasSkinningData() const noexcept
{
    return !m_joint_matrices.empty();
}

u32 Mesh::GetSkinJointOffset(u32 skin_index) const noexcept
{
    if (skin_index >= m_skin_joint_offsets.size())
    {
        return 0;
    }

    return m_skin_joint_offsets[skin_index];
}

u32 Mesh::GetSkinJointCount(u32 skin_index) const noexcept
{
    if (skin_index >= m_skins.size())
    {
        return 0;
    }

    return static_cast<u32>(m_skins[skin_index].joint_matrices.size());
}

} // namespace Horizon
