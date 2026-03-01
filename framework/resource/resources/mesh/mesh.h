#pragma once

#include <limits>
#include <string>
#include <thread>
#include <unordered_map>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>

#include <core/definations.h>
#include <core/math.h>

#include <rhi/buffer.h>
#include <rhi/pipeline.h>
#include <rhi/rhi.h>
#include <rhi/semaphore.h>
#include <rhi/texture.h>

#include <scene/material/material_description.h>

#include "../vertex/vertexdescription.h"

namespace Horizon
{

static thread_local Assimp::Importer assimp_importer;
// mesh description for horizon runtime

struct MeshPrimitive
{
    u32 index_offset{};
    u32 index_count{};
    u32 material_id{};
    i32 skin_index{-1};
    u32 node_index{std::numeric_limits<u32>::max()};
};

struct Node
{
    u32 parent{std::numeric_limits<u32>::max()};
    std::string name{};
    Math::float3 translation{};
    Math::float3 scale{1.0f, 1.0f, 1.0f};
    Math::float4 rotation{0.0f, 0.0f, 0.0f, 1.0f};
    Math::float4x4 local_matrix{Math::float4x4::Identity};
    Math::float4x4 model_matrix{};
    std::vector<u32> childs{};
    std::vector<u32> mesh_primitives{};
    const Math::float4x4 &GetModelMatrix() const
    {
        return model_matrix;
    }
};

struct MeshDesc
{
    u32 vertex_attribute_flag{};
    EMeshAssetFormat mesh_format{};
};

struct MeshSkin
{
    std::string name{};
    std::vector<u32> joint_node_indices{};
    std::vector<Math::float4x4> inverse_bind_matrices{};
    std::vector<Math::float4x4> joint_matrices{};
};

struct MeshNodeAnimationChannel
{
    u32 node_index{std::numeric_limits<u32>::max()};
    std::vector<f32> position_times{};
    std::vector<Math::float3> position_values{};
    std::vector<f32> rotation_times{};
    std::vector<Math::float4> rotation_values{};
    std::vector<f32> scale_times{};
    std::vector<Math::float3> scale_values{};
};

struct MeshAnimationClip
{
    std::string name{};
    f32 duration{0.0f};
    f32 ticks_per_second{25.0f};
    f32 current_time{0.0f};
    std::vector<MeshNodeAnimationChannel> channels{};
};

class Mesh
{
  public:
    Mesh(const MeshDesc &desc, const char *path) noexcept;
    ~Mesh() noexcept;

    void Load();
    void UpdateAnimation(f32 delta_time_seconds);

    const std::vector<Node> &GetNodes() const noexcept;
    const std::vector<Math::float4x4> &GetJointMatrices() const noexcept;
    bool HasSkinningData() const noexcept;
    u32 GetSkinJointOffset(u32 skin_index) const noexcept;
    u32 GetSkinJointCount(u32 skin_index) const noexcept;

    Material &GetMaterial(u32 index) noexcept
    {
        return materials[index];
    }

    std::vector<Material> &GetMaterials() noexcept
    {
        return materials;
    }

  private:
    u32 ProcessNode(const aiScene *scene, aiNode *node, u32 parent_index, const Math::float4x4 &parent_model_matrx);

    void ProcessMaterials(const aiScene *scene);
    void ProcessAnimations(const aiScene *scene);
    void UpdateNodeMatrices();
    void UpdateSkinMatrices();
    void FlattenJointMatrices();

  public:
    u32 vertex_attribute_flag{};
    const char *m_path{};

    std::vector<MeshPrimitive> m_mesh_primitives{};
    std::vector<Vertex> m_vertices{};
    std::vector<Index> m_indices{};
    std::vector<Node> m_nodes{};
    std::vector<Material> materials{};
    std::vector<MeshSkin> m_skins{};
    std::vector<u32> m_skin_joint_offsets{};
    std::vector<Math::float4x4> m_joint_matrices{};
    std::vector<MeshAnimationClip> m_animations{};
    u32 m_active_animation{0};

    Math::float4x4 transform = Math::float4x4::Identity;

    u32 vertex_buffer_index;
    u32 index_buffer_index;

  private:
    std::unordered_map<std::string, u32> m_node_name_to_index{};
};

} // namespace Horizon
