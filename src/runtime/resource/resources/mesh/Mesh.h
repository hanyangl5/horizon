#pragma once

#include <thread>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>

#include <runtime/core/math/math.h>
#include <runtime/core/utils/definations.h>

#include <runtime/rhi/buffer.h>
#include <runtime/rhi/pipeline.h>
#include <runtime/rhi/rhi.h>
#include <runtime/rhi/semaphore.h>
#include <runtime/rhi/texture.h>

#include <runtime/scene/material/materialdescription.h>

#include "../vertex/vertexdescription.h"

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
    Mesh(const MeshDesc &desc, const std::filesystem::path &path) noexcept;
    ~Mesh() noexcept;

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

    std::vector<MeshPrimitive> m_mesh_primitives{};
    std::vector<Vertex> m_vertices{};
    std::vector<Index> m_indices{};
    std::vector<Node> m_nodes{};
    std::vector<Material> materials{};

    Math::float4x4 transform = Math::float4x4::Identity;

    u32 vertex_buffer_index;
    u32 index_buffer_index;
};

} // namespace Horizon