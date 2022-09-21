#include "Mesh.h"

#include <algorithm>

#include <meshoptimizer.h>

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <runtime/core/log/Log.h>
#include <runtime/function/rhi/RHI.h>

namespace Horizon {

using namespace Assimp;

Mesh::Mesh(const MeshDesc &desc) noexcept { vertex_attribute_flag = desc.vertex_attribute_flag; }

Mesh::~Mesh() noexcept {
    // delete
    for (auto &m : materials) {
        for (auto &[type, tex] : m.material_textures) {
            stbi_image_free(tex.data);
        }
    }
}

void Mesh::CreateGpuResources(RHI::RHI *rhi) {

    BufferCreateInfo vertex_buffer_create_info{};
    vertex_buffer_create_info.size = GetVerticesCount() * sizeof(Vertex);
    vertex_buffer_create_info.descriptor_type = DescriptorType::DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vertex_buffer_create_info.initial_state = ResourceState::RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    m_vertex_buffer = rhi->CreateBuffer(vertex_buffer_create_info);

    BufferCreateInfo index_buffer_create_info{};
    index_buffer_create_info.size = GetIndicesCount() * sizeof(Index);
    index_buffer_create_info.descriptor_type = DescriptorType::DESCRIPTOR_TYPE_INDEX_BUFFER;
    index_buffer_create_info.initial_state = ResourceState::RESOURCE_STATE_INDEX_BUFFER;
    m_index_buffer = rhi->CreateBuffer(index_buffer_create_info);

    for (auto &m : materials) {
        for (auto &[type, tex] : m.material_textures) {
            TextureCreateInfo create_info{};
            create_info.width = tex.width;
            create_info.height = tex.height;
            create_info.depth = 1;
            create_info.texture_format = TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM; // TOOD: optimize format
            create_info.texture_type = TextureType::TEXTURE_TYPE_2D;                  // TODO: cubemap?
            create_info.descriptor_type = DescriptorType::DESCRIPTOR_TYPE_TEXTURE;
            create_info.initial_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;

            tex.texture = rhi->CreateTexture(create_info);
        }
    }
}

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
        n.mesh_primitives[i] = &m_mesh_primitives[node->mMeshes[i]];
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
    std::vector<std::string> strs;
    materials.resize(scene->mNumMaterials);

    aiReturn ret;

    for (u32 i = 0; i < scene->mNumMaterials; i++) {

        // maybe useful?
        aiShadingMode shadingMode;
        scene->mMaterials[i]->Get(AI_MATKEY_SHADING_MODEL, shadingMode);

        aiString temp_path;

        // base color textures
        for (uint32_t t = 0; t < scene->mMaterials[i]->GetTextureCount(aiTextureType::aiTextureType_BASE_COLOR); t++) {
            ret = scene->mMaterials[i]->GetTexture(aiTextureType::aiTextureType_BASE_COLOR, t, &temp_path);
            assert(ret == aiReturn_SUCCESS);
            std::filesystem::path abs_path = m_path.parent_path();
            abs_path /= temp_path.C_Str();
            materials[i].material_textures.emplace(MaterialTextureType::BASE_COLOR, abs_path);
        }
        // normal
        for (uint32_t t = 0; t < scene->mMaterials[i]->GetTextureCount(aiTextureType::aiTextureType_NORMALS); t++) {
            ret = scene->mMaterials[i]->GetTexture(aiTextureType::aiTextureType_NORMALS, t, &temp_path);
            assert(ret == aiReturn_SUCCESS);
            std::filesystem::path abs_path = m_path.parent_path();
            abs_path /= temp_path.C_Str();
            materials[i].material_textures.emplace(MaterialTextureType::NORMAL, abs_path);
        }
        // metallic roughness
        for (uint32_t t = 0; t < scene->mMaterials[i]->GetTextureCount(aiTextureType::aiTextureType_DIFFUSE_ROUGHNESS);
             t++) {
            ret = scene->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, t, &temp_path);
            assert(ret == aiReturn_SUCCESS);
            std::filesystem::path abs_path = m_path.parent_path();
            abs_path /= temp_path.C_Str();
            materials[i].material_textures.emplace(MaterialTextureType::METALLIC_ROUGHTNESS, abs_path);
        }

        //for (uint32_t t = 0; t < scene->mMaterials[i]->GetTextureCount(aiTextureType::aiTextureType_EMISSIVE); t++) {
        //    ret = scene->mMaterials[i]->GetTexture(aiTextureType_EMISSIVE, t, &temp_path);
        //    assert(ret == aiReturn_SUCCESS);
        //    std::filesystem::path abs_path = m_path;
        //    abs_path /= temp_path.C_Str();
        //    materials[i].material_textures.emplace(MaterialTextureType::EMISSIVE, abs_path);
        //}
    }

    // load textures

    for (auto &m : materials) {
        for (auto &[type, tex] : m.material_textures) {
            int width, height, channels;
            auto path = tex.url.string();
            u8 *data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
            assert(("failed to load texture", data != nullptr));
            tex.width = width;
            tex.height = height;
            tex.channels = channels;
            tex.data = data;
        }
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

void Mesh::LoadMesh(const std::filesystem::path &path) {
    m_path = path;
    // check mesh if loaded
    if (!m_vertices.empty()) {
        LOG_ERROR("mesh already loaded");
        return;
    }
    // And have it read the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll
    // probably to request more postprocessing than we do in this example.
    const aiScene *scene =
        assimp_importer.ReadFile(path.string().c_str(), aiProcess_CalcTangentSpace | aiProcess_Triangulate |
                                                            aiProcess_GenSmoothNormals | aiProcess_FlipUVs);

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
            if (vertex_attribute_flag & VertexAttributeType::UV0 && mesh->HasTextureCoords(0)) {
                memcpy(&vertex.uv0, &mesh->mTextureCoords[0][v], sizeof(Math::float2));
            }
            if (vertex_attribute_flag & VertexAttributeType::UV1 && mesh->HasTextureCoords(1)) {
                memcpy(&vertex.uv1, &mesh->mTextureCoords[1][v], sizeof(Math::float2));
            }
            m_vertices.emplace_back(vertex);
        }
        m_mesh_primitives[m].index_offset = static_cast<u32>(m_indices.size());
        m_mesh_primitives[m].index_count = mesh->mNumFaces * 3;
        m_mesh_primitives[m].material_id = mesh->mMaterialIndex;
        memcpy(&m_mesh_primitives[m].aabb, &mesh->mAABB,sizeof(AABB));

        for (u32 f = 0; f < mesh->mNumFaces; f++) {
            // use global indices
            m_indices.emplace_back(index_offset + mesh->mFaces[f].mIndices[0]);
            m_indices.emplace_back(index_offset + mesh->mFaces[f].mIndices[1]);
            m_indices.emplace_back(index_offset + mesh->mFaces[f].mIndices[2]);
        }
        index_offset = m_vertices.size();
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

RHI::Buffer *Mesh::GetVertexBuffer() noexcept { return m_vertex_buffer.get(); }

RHI::Buffer *Mesh::GetIndexBuffer() noexcept { return m_index_buffer.get(); }

void Mesh::UploadResources(RHI::CommandList *transfer) {

    // upload vertex and index buffer
    transfer->UpdateBuffer(m_vertex_buffer.get(), m_vertices.data(), m_vertex_buffer->m_size);
    transfer->UpdateBuffer(m_index_buffer.get(), m_indices.data(), m_index_buffer->m_size);

    // upload textures
    // insert barrier for texture for layout transition
   BarrierDesc barrier_desc{};
    
    for (auto &m : materials) {
        for (auto &[type, tex] : m.material_textures) {
            TextureUpdateDesc desc{};
            desc.data = tex.data;
            transfer->UpdateTexture(tex.texture.get(), desc);
            //TextureBarrierDesc texture_barrier{};
            //texture_barrier.src_state = ResourceState::RESOURCE_STATE_UNDEFINED;
            //texture_barrier.dst_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
            //texture_barrier.texture = tex.texture.get();
            //barrier_desc.texture_memory_barriers.push_back(texture_barrier);
        }
    }
    transfer->InsertBarrier(barrier_desc);
}

const std::vector<Node> &Mesh::GetNodes() const noexcept { return m_nodes; }

} // namespace Horizon
