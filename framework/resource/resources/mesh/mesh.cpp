#include "mesh.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>

#include <assimp/GltfMaterial.h>
#include <assimp/anim.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <core/log.h>

#include <resource/resource_loader/texture/texture_loader.h>

namespace Horizon
{

using namespace Assimp;

namespace
{
constexpr u32 INVALID_NODE_INDEX = std::numeric_limits<u32>::max();

Math::float4x4 AiToMathMatrix(const aiMatrix4x4 &m)
{
    return Math::float4x4{m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2,
                          m.a3, m.b3, m.c3, m.d3, m.a4, m.b4, m.c4, m.d4};
}

Math::float3 AiToFloat3(const aiVector3D &v)
{
    return Math::float3(v.x, v.y, v.z);
}

Math::float4 AiToFloat4(const aiQuaternion &q)
{
    return Math::float4(q.x, q.y, q.z, q.w);
}

aiMatrix4x4 BuildAiMatrixFromTRS(const Math::float3 &translation, const Math::float4 &rotation,
                                 const Math::float3 &scale)
{
    aiMatrix4x4 translation_matrix;
    aiMatrix4x4::Translation(aiVector3D(translation.x, translation.y, translation.z), translation_matrix);

    aiMatrix4x4 scale_matrix;
    aiMatrix4x4::Scaling(aiVector3D(scale.x, scale.y, scale.z), scale_matrix);

    aiQuaternion quat;
    quat.x = rotation.x;
    quat.y = rotation.y;
    quat.z = rotation.z;
    quat.w = rotation.w;
    quat.Normalize();
    aiMatrix4x4 rotation_matrix(quat.GetMatrix());

    return translation_matrix * rotation_matrix * scale_matrix;
}

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

            aiQuaternion q0(values[i].w, values[i].x, values[i].y, values[i].z);
            aiQuaternion q1(values[i + 1].w, values[i + 1].x, values[i + 1].y, values[i + 1].z);
            aiQuaternion out{};
            aiQuaternion::Interpolate(out, q0, q1, factor);
            out.Normalize();
            return AiToFloat4(out);
        }
    }
    return values.back();
}

void NormalizeJointWeights(Math::float4 &weights)
{
    const f32 weight_sum = weights.x + weights.y + weights.z + weights.w;
    if (weight_sum > 0.0f)
    {
        const f32 inv = 1.0f / weight_sum;
        weights.x *= inv;
        weights.y *= inv;
        weights.z *= inv;
        weights.w *= inv;
    }
}
} // namespace

Mesh::Mesh(const MeshDesc &desc, const char *path) noexcept
    : vertex_attribute_flag(desc.vertex_attribute_flag), m_asset_path(path)
{
}

Mesh::~Mesh() noexcept
{
}

u32 Mesh::ProcessNode(const aiScene *scene, aiNode *node, u32 parent_index, const Math::float4x4 &parent_model_matrx)
{
    (void)scene;
    if (!node)
    {
        return INVALID_NODE_INDEX;
    }

    const u32 index = static_cast<u32>(m_nodes.size());
    m_nodes.emplace_back();
    m_nodes[index].parent = parent_index;
    m_nodes[index].name = node->mName.C_Str();
    m_nodes[index].local_matrix = AiToMathMatrix(node->mTransformation);
    m_nodes[index].model_matrix = m_nodes[index].local_matrix * parent_model_matrx;

    aiVector3D scaling(1.0f, 1.0f, 1.0f);
    aiVector3D translation(0.0f, 0.0f, 0.0f);
    aiQuaternion rotation;
    node->mTransformation.Decompose(scaling, rotation, translation);
    m_nodes[index].translation = AiToFloat3(translation);
    m_nodes[index].scale = AiToFloat3(scaling);
    m_nodes[index].rotation = AiToFloat4(rotation);

    m_node_name_to_index[m_nodes[index].name] = index;

    m_nodes[index].mesh_primitives.reserve(node->mNumMeshes);
    for (u32 i = 0; i < node->mNumMeshes; i++)
    {
        const u32 primitive_index = node->mMeshes[i];
        m_nodes[index].mesh_primitives.push_back(primitive_index);
        if (primitive_index < m_mesh_primitives.size() &&
            m_mesh_primitives[primitive_index].node_index == INVALID_NODE_INDEX)
        {
            m_mesh_primitives[primitive_index].node_index = index;
        }
    }

    m_nodes[index].childs.reserve(node->mNumChildren);
    for (u32 i = 0; i < node->mNumChildren; i++)
    {
        const u32 child_index = ProcessNode(scene, node->mChildren[i], index, m_nodes[index].model_matrix);
        if (child_index != INVALID_NODE_INDEX)
        {
            m_nodes[index].childs.push_back(child_index);
        }
    }

    return index;
}

void Mesh::ProcessMaterials(const aiScene *scene)
{

    std::vector<aiMaterial *> ms;
    materials.resize(scene->mNumMaterials);
    [[maybe_unused]] aiReturn ret;
    for (u32 i = 0; i < scene->mNumMaterials; i++)
    {
        int shading_model = 0;
        scene->mMaterials[i]->Get(AI_MATKEY_SHADING_MODEL, shading_model);
        if (shading_model == aiShadingMode_Unlit)
        {
            materials[i].shading_model = ShadingModel::SHADING_MODEL_UNLIT;
        }
        // shading model
        bool two_side;
        scene->mMaterials[i]->Get(AI_MATKEY_TWOSIDED, two_side);
        if (two_side == true)
        {
            materials[i].shading_model = ShadingModel::SHADING_MODEL_TWO_SIDE;
        }
        // blend state

        aiString alphaMode;
        scene->mMaterials[i]->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode);
        if (strcmp(alphaMode.C_Str(), "BLEND") == 0)
        {
            materials[i].blend_state = BlendState::BLEND_STATE_TRANSPARENT;
        }
        else if (strcmp(alphaMode.C_Str(), "MASK") == 0)
        {
            materials[i].blend_state = BlendState::BLEND_STATE_MASKED;
        }
        else if (strcmp(alphaMode.C_Str(), "OPAQUE") == 0)
        {
            materials[i].blend_state = BlendState::BLEND_STATE_OPAQUE;
        }

        aiString temp_path;
        // base color textures
        for (uint32_t t = 0; t < scene->mMaterials[i]->GetTextureCount(aiTextureType::aiTextureType_BASE_COLOR); t++)
        {
            ret = scene->mMaterials[i]->GetTexture(aiTextureType::aiTextureType_BASE_COLOR, t, &temp_path);
            assert(ret == aiReturn_SUCCESS);
            Path abs_path = Path(m_asset_path).parent_path();
            abs_path /= temp_path.C_Str();
            materials[i].material_textures.emplace(MaterialTextureType::BASE_COLOR, abs_path);
            materials[i].material_params.param_bitmask |= HAS_BASE_COLOR;
        }
        // normal
        for (uint32_t t = 0; t < scene->mMaterials[i]->GetTextureCount(aiTextureType::aiTextureType_NORMALS); t++)
        {
            ret = scene->mMaterials[i]->GetTexture(aiTextureType::aiTextureType_NORMALS, t, &temp_path);
            assert(ret == aiReturn_SUCCESS);
            Path abs_path = Path(m_asset_path).parent_path();
            abs_path /= temp_path.C_Str();
            materials[i].material_textures.emplace(MaterialTextureType::NORMAL, abs_path);
            materials[i].material_params.param_bitmask |= HAS_NORMAL;
        }
        // metallic roughness
        for (uint32_t t = 0; t < scene->mMaterials[i]->GetTextureCount(aiTextureType::aiTextureType_DIFFUSE_ROUGHNESS);
             t++)
        {
            ret = scene->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, t, &temp_path);
            assert(ret == aiReturn_SUCCESS);
            Path abs_path = Path(m_asset_path).parent_path();
            abs_path /= temp_path.C_Str();
            materials[i].material_textures.emplace(MaterialTextureType::METALLIC_ROUGHTNESS, abs_path);
            materials[i].material_params.param_bitmask |= HAS_METALLIC_ROUGHNESS;
        }

        for (uint32_t t = 0; t < scene->mMaterials[i]->GetTextureCount(aiTextureType::aiTextureType_EMISSIVE); t++)
        {
            ret = scene->mMaterials[i]->GetTexture(aiTextureType_EMISSIVE, t, &temp_path);
            assert(ret == aiReturn_SUCCESS);
            Path abs_path = Path(m_asset_path).parent_path();
            abs_path /= temp_path.C_Str();
            materials[i].material_textures.emplace(MaterialTextureType::EMISSIVE, abs_path);
            materials[i].material_params.param_bitmask |= HAS_EMISSIVE;
        }
    }

    // async load material textures
    auto &mats = this->materials;
    std::vector<std::thread> threads;
    const u32 max_available_thread = std::max(1u, std::thread::hardware_concurrency() - 1);
    threads.reserve(max_available_thread);

    const u32 block_image_size = (u32)mats.size() / max_available_thread + 1;

    for (u32 i = 0; i < max_available_thread; ++i)
    {
        threads.emplace_back([&mats, block_image_size, i]() {
            for (u32 j = i * block_image_size; j < (i + 1) * block_image_size && j < mats.size(); ++j)
            {
                for (auto &[type, tex] : mats[j].material_textures)
                {
                    (void)type;
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

void Mesh::ProcessAnimations(const aiScene *scene)
{
    m_animations.clear();
    if (!scene || scene->mNumAnimations == 0)
    {
        return;
    }

    m_animations.reserve(scene->mNumAnimations);
    for (u32 animation_index = 0; animation_index < scene->mNumAnimations; ++animation_index)
    {
        const aiAnimation *src_animation = scene->mAnimations[animation_index];
        if (!src_animation)
        {
            continue;
        }

        MeshAnimationClip clip{};
        clip.name = src_animation->mName.C_Str();
        clip.duration = static_cast<f32>(src_animation->mDuration);
        clip.ticks_per_second =
            src_animation->mTicksPerSecond > 0.0 ? static_cast<f32>(src_animation->mTicksPerSecond) : 25.0f;
        clip.current_time = 0.0f;
        clip.channels.reserve(src_animation->mNumChannels);

        for (u32 channel_index = 0; channel_index < src_animation->mNumChannels; ++channel_index)
        {
            const aiNodeAnim *src_channel = src_animation->mChannels[channel_index];
            if (!src_channel)
            {
                continue;
            }

            const auto node_it = m_node_name_to_index.find(src_channel->mNodeName.C_Str());
            if (node_it == m_node_name_to_index.end())
            {
                continue;
            }

            MeshNodeAnimationChannel channel{};
            channel.node_index = node_it->second;

            channel.position_times.reserve(src_channel->mNumPositionKeys);
            channel.position_values.reserve(src_channel->mNumPositionKeys);
            for (u32 i = 0; i < src_channel->mNumPositionKeys; ++i)
            {
                channel.position_times.push_back(static_cast<f32>(src_channel->mPositionKeys[i].mTime));
                channel.position_values.push_back(AiToFloat3(src_channel->mPositionKeys[i].mValue));
            }

            channel.rotation_times.reserve(src_channel->mNumRotationKeys);
            channel.rotation_values.reserve(src_channel->mNumRotationKeys);
            for (u32 i = 0; i < src_channel->mNumRotationKeys; ++i)
            {
                channel.rotation_times.push_back(static_cast<f32>(src_channel->mRotationKeys[i].mTime));
                channel.rotation_values.push_back(AiToFloat4(src_channel->mRotationKeys[i].mValue));
            }

            channel.scale_times.reserve(src_channel->mNumScalingKeys);
            channel.scale_values.reserve(src_channel->mNumScalingKeys);
            for (u32 i = 0; i < src_channel->mNumScalingKeys; ++i)
            {
                channel.scale_times.push_back(static_cast<f32>(src_channel->mScalingKeys[i].mTime));
                channel.scale_values.push_back(AiToFloat3(src_channel->mScalingKeys[i].mValue));
            }

            clip.channels.emplace_back(std::move(channel));
        }

        if (!clip.channels.empty() && clip.duration > 0.0f)
        {
            m_animations.emplace_back(std::move(clip));
        }
    }

    if (!m_animations.empty())
    {
        m_active_animation = 0;
    }
}

void Mesh::UpdateNodeMatrices()
{
    for (u32 i = 0; i < m_nodes.size(); ++i)
    {
        auto &node = m_nodes[i];
        node.local_matrix = AiToMathMatrix(BuildAiMatrixFromTRS(node.translation, node.rotation, node.scale));

        if (node.parent == INVALID_NODE_INDEX)
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
        if (primitive.node_index == INVALID_NODE_INDEX || primitive.node_index >= m_nodes.size())
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
            if (node_index == INVALID_NODE_INDEX || node_index >= m_nodes.size())
            {
                skin.joint_matrices[joint_index] = Math::float4x4::Identity;
                continue;
            }

            // Matrices are stored in row-vector convention in CPU memory (transposed from source data),
            // and shaders consume them via mul(M, v), which effectively applies row-vector composition.
            // So skinning order must be inverseBind -> jointGlobal -> inverseMesh.
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

    const aiScene *scene =
        assimp_importer.ReadFile(Path(m_asset_path).string().c_str(),
                                 (u32)(aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                                       aiProcess_FlipUVs | aiProcess_GenBoundingBoxes | aiProcess_CalcTangentSpace));

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        LOG_ERROR("failed to load mesh: {}", assimp_importer.GetErrorString());
        return;
    }

    m_mesh_primitives.clear();
    m_mesh_primitives.resize(scene->mNumMeshes);
    m_nodes.clear();
    m_node_name_to_index.clear();
    m_skins.clear();
    m_skin_joint_offsets.clear();
    m_joint_matrices.clear();
    m_animations.clear();
    m_active_animation = 0;

    ProcessNode(scene, scene->mRootNode, INVALID_NODE_INDEX, Math::float4x4::Identity);

    u32 index_offset = 0;
    for (u32 mesh_index = 0; mesh_index < scene->mNumMeshes; ++mesh_index)
    {
        const aiMesh *mesh = scene->mMeshes[mesh_index];
        if (!mesh)
        {
            continue;
        }

        std::vector<Math::float4> vertex_joint_indices(mesh->mNumVertices, Math::float4(0.0f, 0.0f, 0.0f, 0.0f));
        std::vector<Math::float4> vertex_joint_weights(mesh->mNumVertices, Math::float4(0.0f, 0.0f, 0.0f, 0.0f));

        i32 skin_index = -1;
        if (mesh->HasBones() && mesh->mNumBones > 0)
        {
            skin_index = static_cast<i32>(m_skins.size());
            m_skins.emplace_back();
            auto &skin = m_skins.back();
            skin.name = mesh->mName.C_Str();
            skin.joint_node_indices.resize(mesh->mNumBones, INVALID_NODE_INDEX);
            skin.inverse_bind_matrices.resize(mesh->mNumBones);
            skin.joint_matrices.resize(mesh->mNumBones, Math::float4x4::Identity);

            auto assign_joint_weight = [&vertex_joint_indices, &vertex_joint_weights](u32 vertex_id, u32 joint_id,
                                                                                      f32 weight) {
                auto &indices = vertex_joint_indices[vertex_id];
                auto &weights = vertex_joint_weights[vertex_id];
                f32 *weight_slots = &weights.x;
                f32 *index_slots = &indices.x;

                for (u32 slot = 0; slot < 4; ++slot)
                {
                    if (weight_slots[slot] == 0.0f)
                    {
                        weight_slots[slot] = weight;
                        index_slots[slot] = static_cast<f32>(joint_id);
                        return;
                    }
                }

                u32 min_slot = 0;
                for (u32 slot = 1; slot < 4; ++slot)
                {
                    if (weight_slots[slot] < weight_slots[min_slot])
                    {
                        min_slot = slot;
                    }
                }
                if (weight > weight_slots[min_slot])
                {
                    weight_slots[min_slot] = weight;
                    index_slots[min_slot] = static_cast<f32>(joint_id);
                }
            };

            for (u32 bone_index = 0; bone_index < mesh->mNumBones; ++bone_index)
            {
                const aiBone *bone = mesh->mBones[bone_index];
                if (!bone)
                {
                    continue;
                }

                const auto joint_node_it = m_node_name_to_index.find(bone->mName.C_Str());
                if (joint_node_it != m_node_name_to_index.end())
                {
                    skin.joint_node_indices[bone_index] = joint_node_it->second;
                }
                else
                {
                    LOG_WARN("joint '{}' is missing in node hierarchy for mesh '{}'", bone->mName.C_Str(), m_asset_path.string());
                }

                skin.inverse_bind_matrices[bone_index] = AiToMathMatrix(bone->mOffsetMatrix);

                for (u32 weight_index = 0; weight_index < bone->mNumWeights; ++weight_index)
                {
                    const aiVertexWeight &weight = bone->mWeights[weight_index];
                    if (weight.mVertexId >= mesh->mNumVertices)
                    {
                        continue;
                    }
                    assign_joint_weight(weight.mVertexId, bone_index, weight.mWeight);
                }
            }

            for (u32 v = 0; v < mesh->mNumVertices; ++v)
            {
                NormalizeJointWeights(vertex_joint_weights[v]);
            }
        }

        for (u32 vertex_index = 0; vertex_index < mesh->mNumVertices; ++vertex_index)
        {
            Vertex vertex{};
            memcpy(&vertex.pos, &mesh->mVertices[vertex_index], sizeof(Math::float3));

            if (vertex_attribute_flag & VertexAttributeType::NORMAL && mesh->HasNormals())
            {
                memcpy(&vertex.normal, &mesh->mNormals[vertex_index], sizeof(Math::float3));
            }
            if (vertex_attribute_flag & VertexAttributeType::UV0 && mesh->HasTextureCoords(0))
            {
                memcpy(&vertex.uv0, &mesh->mTextureCoords[0][vertex_index], sizeof(Math::float2));
            }
            if (vertex_attribute_flag & VertexAttributeType::UV1 && mesh->HasTextureCoords(1))
            {
                memcpy(&vertex.uv1, &mesh->mTextureCoords[1][vertex_index], sizeof(Math::float2));
            }
            if (vertex_attribute_flag & VertexAttributeType::TANGENT && mesh->HasTangentsAndBitangents())
            {
                memcpy(&vertex.tangent, &mesh->mTangents[vertex_index], sizeof(Math::float3));
            }

            if (skin_index >= 0)
            {
                vertex.joint_indices = vertex_joint_indices[vertex_index];
                vertex.joint_weights = vertex_joint_weights[vertex_index];
            }

            m_vertices.emplace_back(vertex);
        }

        m_mesh_primitives[mesh_index].index_offset = static_cast<u32>(m_indices.size());
        m_mesh_primitives[mesh_index].index_count = mesh->mNumFaces * 3;
        m_mesh_primitives[mesh_index].material_id = mesh->mMaterialIndex;
        m_mesh_primitives[mesh_index].skin_index = skin_index;

        for (u32 face_index = 0; face_index < mesh->mNumFaces; ++face_index)
        {
            m_indices.emplace_back(index_offset + mesh->mFaces[face_index].mIndices[0]);
            m_indices.emplace_back(index_offset + mesh->mFaces[face_index].mIndices[1]);
            m_indices.emplace_back(index_offset + mesh->mFaces[face_index].mIndices[2]);
        }

        index_offset = static_cast<u32>(m_vertices.size());
    }

    m_vertices.shrink_to_fit();
    m_indices.shrink_to_fit();

    ProcessMaterials(scene);
    ProcessAnimations(scene);
    UpdateSkinMatrices();
    FlattenJointMatrices();

    LOG_DEBUG("mesh successfully loaded, {} meshes, {} vertices, {} faces, {} skins, {} animations",
              m_mesh_primitives.size(), m_vertices.size(), m_indices.size(), m_skins.size(), m_animations.size());
    assimp_importer.FreeScene();
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
