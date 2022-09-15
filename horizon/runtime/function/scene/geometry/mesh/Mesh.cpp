#include "Mesh.h"

#include <algorithm>

#include <meshoptimizer.h>

#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <runtime/core/log/Log.h>

namespace Horizon {

using namespace Assimp;

Mesh::Mesh(const MeshDesc &desc) noexcept { vertex_attribute_flag = desc.vertex_attribute_flag; }

Mesh::~Mesh() noexcept {
    // delete
}

void Mesh::ProcessNode(const aiScene *scene, aiNode *node, u32 index) {
    if (!node) {
        return;
    }
    auto &n = m_nodes[index];
    n.mesh_primitives.resize(node->mNumMeshes);

    for (u32 i = 0; i < node->mNumMeshes; i++) {
        n.mesh_primitives[i] = &m_mesh_primitives[node->mMeshes[i]];
    }

    n.childs.resize(node->mNumChildren);

    for (u32 i = 0; i < node->mNumChildren; i++) {
        u32 child_node_index = index + i + 1;
        n.childs[i] = child_node_index;
        m_nodes[child_node_index].parent = index;
        ProcessNode(scene, node->mChildren[i], child_node_index);
    }
}

// TODO:
void Mesh::GenerateMeshCluster() {
    const size_t max_vertices = 64;
    const size_t max_triangles = 124;
    const float cone_weight = 0.0f;

    size_t max_meshlets = meshopt_buildMeshletsBound(m_indices.size(), max_vertices, max_triangles);
    std::vector<meshopt_Meshlet> meshlets(max_meshlets);
    std::vector<unsigned int> meshlet_vertices(max_meshlets * max_vertices);
    std::vector<unsigned char> meshlet_triangles(max_meshlets * max_triangles * 3);

    size_t meshlet_count = meshopt_buildMeshlets(
        meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(), m_indices.data(), m_indices.size(),
        &m_vertices[0].pos.x, m_vertices.size(), sizeof(Vertex), max_vertices, max_triangles, cone_weight);
}

u32 SubNodeCount(const aiNode *node) noexcept {
    int n = node->mNumChildren;

    for (u32 i = 0; i < node->mNumChildren; i++) {
        n += SubNodeCount(node->mChildren[i]);
    }
    return n;
}

u32 CalculateNodeCount(const aiScene *scene) noexcept { return SubNodeCount(scene->mRootNode); }

void Mesh::LoadMesh(const std::string &path) {
    // Assimp::Importer importer;

    // And have it read the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll
    // probably to request more postprocessing than we do in this example.
    const aiScene *scene = assimp_importer.ReadFile(path, aiProcess_CalcTangentSpace | aiProcess_Triangulate |
                                                              aiProcess_GenSmoothNormals | aiProcess_FlipUVs);

    // If the import failed, report it
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_ERROR("failed to load mesh: {}", assimp_importer.GetErrorString());
        return;
    }

    // auto directory = path.substr(0, path.find_last_of('/'));

    m_mesh_primitives.resize(scene->mNumMeshes);

    // TOOD: aync load sub meshes
    for (u32 i = 0; i < scene->mNumMeshes; i++) {

        const auto &mesh = scene->mMeshes[i];
        for (u32 v = 0; v < mesh->mNumVertices; v++) {

            Vertex vertex{};

            memcpy(&vertex.pos, &mesh->mVertices[v], sizeof(Math::float3));

            if (vertex_attribute_flag & VertexAttributeType::NORMAL && mesh->HasNormals()) {
                memcpy(&vertex.normal, &mesh->mNormals[v], sizeof(Math::float3));
            }
            if (vertex_attribute_flag & VertexAttributeType::UV0 && mesh->HasTextureCoords(0)) {
                memcpy(&vertex.uv0, &mesh->mTextureCoords[0][v], sizeof(Math::float2));
            }
            if (vertex_attribute_flag & VertexAttributeType::UV1 && mesh->HasTextureCoords(1)) {
                memcpy(&vertex.uv1, &mesh->mTextureCoords[1][v], sizeof(Math::float2));
            }
            m_vertices.emplace_back(vertex);
        }

        m_mesh_primitives[i].index_offset = static_cast<u32>(m_indices.size());
        m_mesh_primitives[i].index_count = mesh->mNumFaces;

        for (u32 f = 0; f < mesh->mNumFaces; f++) {
            m_indices.emplace_back(mesh->mFaces[i].mIndices[0]);
            m_indices.emplace_back(mesh->mFaces[i].mIndices[1]);
            m_indices.emplace_back(mesh->mFaces[i].mIndices[2]);
        }
    }

    m_vertices.shrink_to_fit();
    m_indices.shrink_to_fit();

    u32 node_count = CalculateNodeCount(scene) + 1;

    m_nodes.resize(node_count);
    ProcessNode(scene, scene->mRootNode, 0);
    LOG_DEBUG("mesh successfully loaded, {} meshes, {} vertices, {} faces", m_mesh_primitives.size(), m_vertices.size(),
              m_indices.size());
}

void Mesh::LoadMesh(BasicGeometry::BasicGeometry basic_geometry) {
    switch (basic_geometry) {
    case Horizon::BasicGeometry::BasicGeometry::QUAD:
        m_vertices = std::vector<Vertex>(BasicGeometry::quad_vertices.begin(), BasicGeometry::quad_vertices.end());
        m_indices = std::vector<Index>(BasicGeometry::quad_indices.begin(), BasicGeometry::quad_indices.end());
        break;
    case Horizon::BasicGeometry::BasicGeometry::TRIANGLE:
        m_vertices =
            std::vector<Vertex>(BasicGeometry::triangle_vertices.begin(), BasicGeometry::triangle_vertices.end());
        m_indices = std::vector<Index>(BasicGeometry::triangle_indices.begin(), BasicGeometry::triangle_indices.end());
        break;
    case Horizon::BasicGeometry::BasicGeometry::CUBE:
        m_vertices = std::vector<Vertex>(BasicGeometry::cube_vertices.begin(), BasicGeometry::cube_vertices.end());

        m_indices = std::vector<Index>(BasicGeometry::cube_indices.begin(), BasicGeometry::cube_indices.end());

        break;
    case Horizon::BasicGeometry::BasicGeometry::SPHERE:
        // load from file
        break;
    case Horizon::BasicGeometry::BasicGeometry::CAPSULE:
        // load from file
        break;
    default:
        break;
    }
    MeshPrimitive m{};
    m.index_count = static_cast<u32>(m_indices.size());
    m.index_offset = 0;

    m_mesh_primitives.push_back(std::move(m));

    Node n{};
    n.parent = 0;
    n.mesh_primitives.emplace_back(&m_mesh_primitives[0]);
    m_nodes.push_back(std::move(n));
}

u32 Mesh::GetVerticesCount() const noexcept { return static_cast<u32>(m_vertices.size()); }

u32 Mesh::GetIndicesCount() const noexcept { return static_cast<u32>(m_indices.size()); }

void *Mesh::GetVerticesData() noexcept { return m_vertices.data(); }

void *Mesh::GetIndicesData() noexcept { return m_indices.data(); }

const std::vector<Node> &Mesh::GetNodes() const noexcept { return m_nodes; }

} // namespace Horizon
