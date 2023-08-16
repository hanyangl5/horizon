#include "Mesh.h"

#include <algorithm>
#include <thread>

#include <assimp/pbrmaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <runtime/core/memory/Memory.h>
#include <runtime/core/log/Log.h>
#include <runtime/rhi/RHI.h>
#include <runtime/resource/resource_loader/texture/TextureLoader.h>

namespace Horizon {

using namespace Assimp;

Mesh::Mesh(const MeshDesc &desc, const std::filesystem::path &path) noexcept
    : vertex_attribute_flag(desc.vertex_attribute_flag), m_path(path) {}

Mesh::~Mesh() noexcept {}

void Mesh::ProcessNode(const aiScene *scene, aiNode *node, u32 index, const Math::float4x4 &parent_model_matrx) {
    if (!node) {
        return;
    }
    auto &n = m_nodes[index];
    // update node model matrix
    auto &t = node->mTransformation;
    Math::float4x4 m(t.a1, t.b1, t.c1, t.d1, t.a2, t.b2, t.c2, t.d2, t.a3, t.b3, t.c3, t.d3, t.a4, t.b4, t.c4, t.d4);
    n.model_matrix = m * parent_model_matrx;

    n.mesh_primitives.resize(node->mNumMeshes);

    for (u32 i = 0; i < node->mNumMeshes; i++) {
        n.mesh_primitives[i] = node->mMeshes[i];
    }

    n.childs.resize(node->mNumChildren);

    for (u32 i = 0; i < node->mNumChildren; i++) {
        u32 child_node_index = index + i + 1;
        n.childs[i] = child_node_index;
        m_nodes[child_node_index].parent = index;
        ProcessNode(scene, node->mChildren[i], child_node_index, n.model_matrix);
    }
}

void Mesh::ProcessMaterials(const aiScene *scene) {

    std::vector<aiMaterial *> ms;
    materials.resize(scene->mNumMaterials);
    [[maybe_unused]]aiReturn ret;
    for (u32 i = 0; i < scene->mNumMaterials; i++) {
        bool unlit;
        scene->mMaterials[i]->Get(AI_MATKEY_GLTF_UNLIT, unlit);
        if (unlit == true) {
            materials[i].shading_model = ShadingModel::SHADING_MODEL_UNLIT;
        }
        // shading model
        bool two_side;
        scene->mMaterials[i]->Get(AI_MATKEY_TWOSIDED, two_side);
        if (two_side == true) {
            materials[i].shading_model = ShadingModel::SHADING_MODEL_TWO_SIDE;
        }
        // blend state

        aiString alphaMode;
        scene->mMaterials[i]->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode);
        if (strcmp(alphaMode.C_Str(), "BLEND") == 0) {
            materials[i].blend_state = BlendState::BLEND_STATE_TRANSPARENT;
        } else if (strcmp(alphaMode.C_Str(), "MASK") == 0) {
            materials[i].blend_state = BlendState::BLEND_STATE_MASKED;
        } else if (strcmp(alphaMode.C_Str(), "OPAQUE") == 0) {
            materials[i].blend_state = BlendState::BLEND_STATE_OPAQUE;
        }

        aiString temp_path;

        // base color textures
        for (uint32_t t = 0; t < scene->mMaterials[i]->GetTextureCount(aiTextureType::aiTextureType_BASE_COLOR); t++) {
            ret = scene->mMaterials[i]->GetTexture(aiTextureType::aiTextureType_BASE_COLOR, t, &temp_path);
            assert(ret == aiReturn_SUCCESS);
            std::filesystem::path abs_path = m_path.parent_path();
            abs_path /= temp_path.C_Str();
            materials[i].material_textures.emplace(MaterialTextureType::BASE_COLOR, abs_path);
            materials[i].material_params.param_bitmask |= HAS_BASE_COLOR;
        }
        // normal
        for (uint32_t t = 0; t < scene->mMaterials[i]->GetTextureCount(aiTextureType::aiTextureType_NORMALS); t++) {
            ret = scene->mMaterials[i]->GetTexture(aiTextureType::aiTextureType_NORMALS, t, &temp_path);
            assert(ret == aiReturn_SUCCESS);
            std::filesystem::path abs_path = m_path.parent_path();
            abs_path /= temp_path.C_Str();
            materials[i].material_textures.emplace(MaterialTextureType::NORMAL, abs_path);
            materials[i].material_params.param_bitmask |= HAS_NORMAL;
        }
        // metallic roughness
        for (uint32_t t = 0; t < scene->mMaterials[i]->GetTextureCount(aiTextureType::aiTextureType_DIFFUSE_ROUGHNESS);
             t++) {
            ret = scene->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, t, &temp_path);
            assert(ret == aiReturn_SUCCESS);
            std::filesystem::path abs_path = m_path.parent_path();
            abs_path /= temp_path.C_Str();
            materials[i].material_textures.emplace(MaterialTextureType::METALLIC_ROUGHTNESS, abs_path);
            materials[i].material_params.param_bitmask |= HAS_METALLIC_ROUGHNESS;
        }

        for (uint32_t t = 0; t < scene->mMaterials[i]->GetTextureCount(aiTextureType::aiTextureType_EMISSIVE); t++) {
            ret = scene->mMaterials[i]->GetTexture(aiTextureType_EMISSIVE, t, &temp_path);
            assert(ret == aiReturn_SUCCESS);
            std::filesystem::path abs_path = m_path.parent_path();
            abs_path /= temp_path.C_Str();
            materials[i].material_textures.emplace(MaterialTextureType::EMISSIVE, abs_path);
            materials[i].material_params.param_bitmask |= HAS_EMISSIVE;
        }
    }

    // async load material textures
    auto &mats = this->materials;
    std::vector<std::thread> threads;
    u32 max_available_thread = std::thread::hardware_concurrency() - 1;
    threads.reserve(max_available_thread);

    u32 block_image_size = (u32)mats.size() / max_available_thread + 1;

    for (u32 i = 0; i < max_available_thread; ++i) {
        threads.emplace_back([&mats, block_image_size, i]() {
            for (u32 j = i * block_image_size; j < (i + 1) * block_image_size && j < mats.size(); ++j) {
                for (auto &[type, tex] : mats[j].material_textures) {
                    tex.texture_data_desc = TextureLoader::Load(tex.url.string().c_str());
                }
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
}

u32 SubNodeCount(const aiNode *node) noexcept {
    int n = node->mNumChildren;

    for (u32 i = 0; i < node->mNumChildren; i++) {
        n += SubNodeCount(node->mChildren[i]);
    }
    return n;
}

u32 CalculateNodeCount(const aiScene *scene) noexcept { return SubNodeCount(scene->mRootNode); }

void Mesh::Load() {

    // check mesh if loaded
    if (!m_vertices.empty()) {
        LOG_ERROR("mesh already loaded");
        return;
    }
    // And have it read the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll
    // probably to request more postprocessing than we do in this example.
    const aiScene *scene = assimp_importer.ReadFile(
        m_path.string().c_str(), (u32)(aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                                     aiProcess_FlipUVs | aiProcess_GenBoundingBoxes | aiProcess_CalcTangentSpace));

    // If the import failed, report it
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_ERROR("failed to load mesh: {}", assimp_importer.GetErrorString());
        return;
    }

    // auto directory = path.substr(0, path.find_last_of('/'));
    u32 index_offset = 0;

    m_mesh_primitives.resize(scene->mNumMeshes);

    // TOOD: aync load sub meshes

    // combine all mesh to a big vertex buffer
    for (u32 m = 0; m < scene->mNumMeshes; m++) {

        const auto &mesh = scene->mMeshes[m];
        for (u32 v = 0; v < mesh->mNumVertices; v++) {

            Vertex vertex{};

            memcpy(&vertex.pos, &mesh->mVertices[v], sizeof(Math::float3));

            if (vertex_attribute_flag & VertexAttributeType::NORMAL && mesh->HasNormals()) {
                memcpy(&vertex.normal, &mesh->mNormals[v], sizeof(Math::float3));
            }
            // if (vertex_attribute_flag & VertexAttributeType::TBN && mesh->HasTangentsAndBitangents()) {
            //     memcpy(&vertex.tbn, &mesh->mTangents[v], sizeof(Math::float3));
            // }
            if (vertex_attribute_flag & VertexAttributeType::UV0 && mesh->HasTextureCoords(0)) {
                memcpy(&vertex.uv0, &mesh->mTextureCoords[0][v], sizeof(Math::float2));
            }
            if (vertex_attribute_flag & VertexAttributeType::UV1 && mesh->HasTextureCoords(1)) {
                memcpy(&vertex.uv1, &mesh->mTextureCoords[1][v], sizeof(Math::float2));
            }
            if (vertex_attribute_flag & VertexAttributeType::TANGENT && mesh->HasTangentsAndBitangents()) {
                memcpy(&vertex.tangent, &mesh->mTangents[v], sizeof(Math::float3));
            }
            m_vertices.emplace_back(vertex);
        }
        m_mesh_primitives[m].index_offset = static_cast<u32>(m_indices.size());
        m_mesh_primitives[m].index_count = mesh->mNumFaces * 3;
        m_mesh_primitives[m].material_id = mesh->mMaterialIndex;
        //memcpy(&m_mesh_primitives[m].aabb, &mesh->mAABB, sizeof(AABB));

        for (u32 f = 0; f < mesh->mNumFaces; f++) {
            // use global indices
            m_indices.emplace_back(index_offset + mesh->mFaces[f].mIndices[0]);
            m_indices.emplace_back(index_offset + mesh->mFaces[f].mIndices[1]);
            m_indices.emplace_back(index_offset + mesh->mFaces[f].mIndices[2]);
        }
        index_offset = (u32)m_vertices.size();
    }

    m_vertices.shrink_to_fit();
    m_indices.shrink_to_fit();

    u32 node_count = CalculateNodeCount(scene) + 1;

    m_nodes.resize(node_count);
    ProcessNode(scene, scene->mRootNode, 0, Math::float4x4::Identity);

    ProcessMaterials(scene);

    LOG_DEBUG("mesh successfully loaded, {} meshes, {} vertices, {} faces", m_mesh_primitives.size(), m_vertices.size(),
              m_indices.size());
    assimp_importer.FreeScene();
}

const std::vector<Node> &Mesh::GetNodes() const noexcept { return m_nodes; }

} // namespace Horizon
