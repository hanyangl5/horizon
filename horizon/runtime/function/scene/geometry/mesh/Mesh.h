#pragma once

#include <array>
#include <memory>
#include <thread>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/RHI.h>

#include "BasicGeometry.h"
#include "VertexDescription.h"

namespace Horizon {

// mesh description for horizon runtime

static thread_local Assimp::Importer assimp_importer;

struct MeshPrimitive {
    u32 index_offset;
    u32 index_count;
    u32 material_id;
};

struct Node {
    u32 parent;
    std::vector<u32> childs;
    std::vector<MeshPrimitive *> mesh_primitives;
};

struct MeshDesc {
    u32 vertex_attribute_flag;
};

class Mesh {
  public:
    Mesh() noexcept = default;

    Mesh(RHI::RHI &rhi, const MeshDesc &desc) noexcept;

    ~Mesh() noexcept;

    void LoadMesh(const std::string &path) noexcept;

    void LoadMesh(BasicGeometry::BasicGeometry basic_geometry) noexcept;

    RHI::Buffer *GetVertexBuffer() noexcept;

    RHI::Buffer *GetIndexBuffer() noexcept;

    const std::vector<Node> &GetNodes() const noexcept;

    // void GenerateMeshLet() noexcept;
  private:
    void ProcessNode(const aiScene *scene, aiNode *node, u32 offset) noexcept;

  private:
    RHI::RHI *m_rhi;
    u32 vertex_attribute_flag{};

    std::vector<MeshPrimitive> m_mesh_primitives{};
    std::vector<Vertex> m_vertices{};
    std::vector<Index> m_indices{};
    std::vector<Node> m_nodes{};
    // Texture* textures;
    // Material* materials;

    Resource<RHI::Buffer> m_vertex_buffer;
    Resource<RHI::Buffer> m_index_buffer;
};
} // namespace Horizon