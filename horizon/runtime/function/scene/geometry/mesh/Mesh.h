#pragma once

#include <array>
#include <memory>
#include <thread>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>

#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/Texture.h>
#include <runtime/function/rhi/Semaphore.h>
#include <runtime/function/rhi/Pipeline.h>

#include <runtime/function/scene/material/MaterialDescription.h>

#include "BasicGeometry.h"
#include "../vertex/VertexDescription.h"
#include "../aabb/AABB.h"

namespace Horizon {

// mesh description for horizon runtime

static thread_local Assimp::Importer assimp_importer;

struct MeshPrimitive {
    u32 index_offset{};
    u32 index_count{};
    u32 material_id{};
    AABB aabb;
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
    Mesh() noexcept = default;
    Mesh(const MeshDesc &desc) noexcept;
    ~Mesh() noexcept;

    Mesh(const Mesh &rhs) noexcept = delete;
    Mesh &operator=(const Mesh &rhs) noexcept = delete;
    Mesh(Mesh &&rhs) noexcept = delete;
    Mesh &operator=(Mesh &&rhs) noexcept = delete;

    void LoadMesh(const std::filesystem::path &path);

    void LoadMesh(BasicGeometry::Shapes basic_geometry);

    u32 GetVerticesCount() const noexcept;

    u32 GetIndicesCount() const noexcept;

    Backend::Buffer *GetVertexBuffer() noexcept;

    Backend::Buffer *GetIndexBuffer() noexcept;

    // receive a recording command list
    void UploadResources(Backend::CommandList *transfer);

    const std::vector<Node> &GetNodes() const noexcept;

    // void GenerateMeshLet() noexcept;

    void CreateGpuResources(Backend::RHI *rhi);

    Material &GetMaterial(u32 index) noexcept { return materials[index]; }

    std::vector<Material> &GetMaterials() noexcept { return materials; }

    void GenerateMipMaps(Backend::Pipeline *pipeline, Backend::CommandList *compute);

  private:
    void ProcessNode(const aiScene *scene, aiNode *node, u32 index, const Math::float4x4 &model_matrx);

    void ProcessMaterials(const aiScene *scene);

    void GenerateMeshCluster();


  private:
    u32 vertex_attribute_flag{};
    std::filesystem::path m_path{};

  public: 
    std::vector<MeshPrimitive> m_mesh_primitives{};
    std::vector<Vertex> m_vertices{};
    std::vector<Index> m_indices{};
    std::vector<Node> m_nodes{};
    std::vector<Material> materials{};

    Math::float4x4 transform = Math::float4x4::Identity;
    // gpu buffer
    Resource<Backend::Buffer> m_vertex_buffer{}, m_index_buffer{};
    // Material* materials
};
} // namespace Horizon