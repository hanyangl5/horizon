#pragma once

#include <array>
#include <memory>
#include <thread>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>

#include "VertexDescription.h"

namespace Horizon {

// mesh description for horizon runtime

static thread_local Assimp::Importer assimp_importer;

struct Node {
    u32 parent;
    std::vector<u32> childs;
    std::vector<u32> meshes;
};

struct MeshPrimitive {
    u32 index_offset;
    u32 index_count;
    u32 material_id;

  private:
    u32 pad0[2];
};

struct MeshDesc {
    u32 vertex_attribute_flag;
};

class Mesh {
  public:
    Mesh(const MeshDesc &desc = {});
    ~Mesh();
    void LoadMesh(const std::string &path);
    void ConvertToClusterdMesh();

  private:
    void ProcessNode(const aiScene *scene, aiNode *node, u32 offset);

  private:
    using Index = u32;
    u32 vertex_attribute_flag{};

    std::vector<MeshPrimitive> m_mesh_primitives{};
    std::vector<Vertex> m_vertices{};
    std::vector<std::array<Index, 3>> m_indices{};
    std::vector<Node> m_nodes{};
    // Texture* textures;
    // Material* materials;

    Math::float4 transform_matrix{};
};
} // namespace Horizon