#pragma once


#include <thread>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>

#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/Pipeline.h>
#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/Semaphore.h>
#include <runtime/function/rhi/Texture.h>

#include <runtime/function/scene/material/MaterialDescription.h>

#include "../aabb/AABB.h"
#include "../vertex/VertexDescription.h"
#include <runtime/function/component/Transform.h>

namespace Horizon {

static thread_local Assimp::Importer assimp_importer;
// mesh description for horizon runtime

struct MeshPrimitive {
    u32 index_offset{};
    u32 index_count{};
    u32 material_id{};
    AABB aabb;
};

struct Node {
    u32 parent{};
    Math::float4x4 model_matrix{};
    Container::Array<u32> childs{};
    Container::Array<u32> mesh_primitives{};
    const Math::float4x4 &GetModelMatrix() const { return model_matrix; }
};

enum class MeshType {
    STATIC,
    DEFORMABLE
};

struct MeshDesc {
    u32 vertex_attribute_flag{};
    MeshType mesh_type = MeshType::STATIC;
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

    const Container::Array<Node> &GetNodes() const noexcept;

    Material &GetMaterial(u32 index) noexcept { return materials[index]; }

    Container::Array<Material> &GetMaterials() noexcept { return materials; }

  private:
    void ProcessNode(const aiScene *scene, aiNode *node, u32 index, const Math::float4x4 &model_matrx);

    void ProcessMaterials(const aiScene *scene);

  public:
    u32 vertex_attribute_flag{};
    std::filesystem::path m_path{};

  public:
    Container::Array<MeshPrimitive> m_mesh_primitives{};
    Container::Array<Vertex> m_vertices{};
    Container::Array<Index> m_indices{};
    Container::Array<Node> m_nodes{};
    Container::Array<Material> materials{};

    Transform transform;

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
//     Container::Array<Vertex> vertices;
//     Container::Array<Index> indices;
//     u32 material_index;
//     Math::float4x4 model_matrix;
//     AABB aabb;
// };
//
// struct HMesh {
//     Container::Array<MeshFragmentResource> mesh_fragments;
//     Container::Array<Material> load;
// };

} // namespace Horizon