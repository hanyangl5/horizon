#include "Mesh.h"

#include <algorithm>

#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <runtime/core/log/Log.h>

namespace Horizon {

using namespace Assimp;

Mesh::Mesh(const MeshDesc &desc) noexcept {
    vertex_attribute_flag = desc.vertex_attribute_flag;
}

Mesh::~Mesh() noexcept {
    // delete
}

void Mesh::ProcessNode(const aiScene *scene, aiNode *node, u32 index) noexcept {
    if (!node) {
        return;
    }
    auto &n = m_nodes[index];
    n.meshes.resize(node->mNumMeshes);

    for (u32 i = 0; i < node->mNumMeshes; i++) {
        n.meshes[i] = node->mMeshes[i];
    }

    n.childs.resize(node->mNumChildren);

    for (u32 i = 0; i < node->mNumChildren; i++) {
        u32 child_node_index = index + i + 1;
        n.childs[i] = child_node_index;
        m_nodes[child_node_index].parent = index;
        ProcessNode(scene, node->mChildren[i], child_node_index);
    }
}

u32 SubNodeCount(const aiNode *node) noexcept {
    int n = node->mNumChildren;

    for (u32 i = 0; i < node->mNumChildren; i++) {
        n += SubNodeCount(node->mChildren[i]);
    }
    return n;
}

u32 CalculateNodeCount(const aiScene *scene) noexcept {
    return SubNodeCount(scene->mRootNode);
}

void Mesh::LoadMesh(const std::string &path) noexcept {
    // Assimp::Importer importer;

    // And have it read the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll
    // probably to request more postprocessing than we do in this example.
    const aiScene *scene = assimp_importer.ReadFile(
        path, aiProcess_CalcTangentSpace | aiProcess_Triangulate |
                  aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);

    // If the import failed, report it
    if (!scene) {
        LOG_ERROR("failed to load mesh: {}", assimp_importer.GetErrorString());
        return;
    }

    // auto directory = path.substr(0, path.find_last_of('/'));

    m_mesh_primitives.resize(scene->mNumMeshes);

    for (u32 i = 0; i < scene->mNumMeshes; i++) {

        const auto &mesh = scene->mMeshes[i];
        for (u32 v = 0; v < mesh->mNumVertices; v++) {

            Vertex vertex{};

            memcpy(&vertex.pos, &mesh->mVertices[v], sizeof(aiVector3D));

            if (vertex_attribute_flag & VertexAttributeType::NORMAL &&
                mesh->HasNormals()) {
            }
            if (vertex_attribute_flag & VertexAttributeType::UV1 &&
                mesh->HasTextureCoords(0)) {
            }

            m_vertices.emplace_back(vertex);
        }

        m_mesh_primitives[i].index_offset = m_indices.size();
        m_mesh_primitives[i].index_count = mesh->mNumFaces;

        for (u32 f = 0; f < mesh->mNumFaces; f++) {
            m_indices.emplace_back(std::array{mesh->mFaces[i].mIndices[0],
                                              mesh->mFaces[i].mIndices[1],
                                              mesh->mFaces[i].mIndices[2]});
        }
    }

    m_vertices.shrink_to_fit();
    m_indices.shrink_to_fit();

    u32 node_count = CalculateNodeCount(scene) + 1;

    m_nodes.resize(node_count);
    ProcessNode(scene, scene->mRootNode, 0);
    LOG_DEBUG("mesh successfully loaded, {} meshes, {} vertices, {} faces",
              m_mesh_primitives.size(), m_vertices.size(), m_indices.size());
}

void Mesh::LoadMesh(BasicGeometry basic_geometry) noexcept {
    switch (basic_geometry) {
    case Horizon::BasicGeometry::QUAD:
        break;
    case Horizon::BasicGeometry::TRIANGLE:
        break;
    case Horizon::BasicGeometry::CUBE:
        break;
    case Horizon::BasicGeometry::SPHERE:
        break;
    case Horizon::BasicGeometry::CAPSULE:
        break;
    default:
        break;
    }
}

} // namespace Horizon
