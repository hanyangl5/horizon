#pragma once

#include <thread>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>

#include <runtime/rhi/Buffer.h>
#include <runtime/rhi/Pipeline.h>
#include <runtime/rhi/RHI.h>
#include <runtime/rhi/Semaphore.h>
#include <runtime/rhi/Texture.h>

#include <runtime/scene/material/MaterialDescription.h>

#include "../vertex/VertexDescription.h"

namespace Horizon {

static thread_local Assimp::Importer assimp_importer;
// mesh description for horizon runtime

struct MeshPrimitive {
    u32 index_offset{};
    u32 index_count{};
    u32 material_id{};
};

struct Node {
    u32 parent{};
    Math::float4x4 model_matrix{};
    std::vector<u32> childs{};
    std::vector<u32> mesh_primitives{};
    const Math::float4x4 &GetModelMatrix() const { return model_matrix; }
};

struct MeshDesc {
    u32 vertex_attribute_flag{};
};

class Mesh {
  public:
    Mesh(const MeshDesc &desc, const std::filesystem::path &path,
         std::pmr::polymorphic_allocator<std::byte> allocator = {}) noexcept;
    ~Mesh() noexcept;

    Mesh(const Mesh &rhs) noexcept = delete;
    Mesh &operator=(const Mesh &rhs) noexcept = delete;
    Mesh(Mesh &&rhs) noexcept = delete;
    Mesh &operator=(Mesh &&rhs) noexcept = delete;

    void Load();

    const std::vector<Node> &GetNodes() const noexcept;

    Material &GetMaterial(u32 index) noexcept { return materials[index]; }

    std::vector<Material> &GetMaterials() noexcept { return materials; }

  private:
    void ProcessNode(const aiScene *scene, aiNode *node, u32 index, const Math::float4x4 &model_matrx);

    void ProcessMaterials(const aiScene *scene);

  public:
    u32 vertex_attribute_flag{};
    std::filesystem::path m_path{};

  public:
    std::vector<MeshPrimitive> m_mesh_primitives{};
    std::vector<Vertex> m_vertices{};
    std::vector<Index> m_indices{};
    std::vector<Node> m_nodes{};
    std::vector<Material> materials{};

    Math::float4x4 transform = Math::float4x4::Identity;

    u32 vertex_buffer_index;
    u32 index_buffer_index;
};

// the smallest unit mesh to process loading, drawcall, material
// class MeshFragment {
//    u32 vertex_buffer_index;
//    u32 index_buffer_index;
//    u32 material_index;
//};

// struct MeshFragmentResource {
//     std::vector<Vertex> vertices;
//     std::vector<Index> indices;
//     u32 material_index;
//     Math::float4x4 model_matrix;
//     AABB aabb;
// };
//
// struct HMesh {
//     std::vector<MeshFragmentResource> mesh_fragments;
//     std::vector<Material> load;
// };

} // namespace Horizon