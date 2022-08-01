#pragma once

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>

#include "VertexDescription.h"

namespace Horizon {

// mesh description for horizon runtime

struct MeshNode {
    u32 index_offset;
    u32 index_count;
    u32 material_id;
    u32 parent_id;
    u32 *childs_id;
    u32 pad0[2];
};

struct MeshDesc {
    u32 vertex_attribute_flag;
};

class Mesh {
  public:
    Mesh(const MeshDesc &desc);
    void LoadMesh(const std::string &path);
    void ConvertToClusterdMesh();

  private:
    using Index = u32;
    u32 vertex_attribute_flag;
    u32 vertices_count;
    u32 indices_count;
    u32 stride;
    f32 aabb[6];
    MeshNode *mesh_node_arr;
    Vertex *vertices;
    Index *indices;
    // Texture* textures;
    // Material* materials;

    Math::float4 transform_matrix;

    u8 *mesh_buffer; // buffer of vertices and indices

    Mesh *root;
    Mesh *child;
};
} // namespace Horizon