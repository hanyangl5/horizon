#pragma once

// Shared mesh loading and material helpers for the Chapter 11 renderer.

#include <cmath>

#include <meshoptimizer.h>

#include "shared/UtilsGLTF.h"

struct DrawIndexedIndirectCommand
{
    uint32_t count;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t  baseVertex;
    uint32_t baseInstance;
};

struct DrawData
{
    uint32_t transformId;
    uint32_t materialId;
};

inline uint32_t packSnorm10(float value)
{
    const int32_t quantized = glm::clamp(static_cast<int32_t>(std::round(glm::clamp(value, -1.0f, 1.0f) * 511.0f)), -512, 511);
    return static_cast<uint32_t>(quantized) & 0x3ffu;
}

inline uint32_t packSnorm10x3(const vec3& value)
{
    const vec3 v = glm::normalize(glm::dot(value, value) > 0.0f ? value : vec3(0.0f, 1.0f, 0.0f));
    return packSnorm10(v.x) | (packSnorm10(v.y) << 10u) | (packSnorm10(v.z) << 20u);
}

inline uint32_t packTangent10x3Handedness(const vec4& value)
{
    const vec3 tangent = glm::normalize(glm::dot(vec3(value), vec3(value)) > 0.0f ? vec3(value) : vec3(1.0f, 0.0f, 0.0f));
    return packSnorm10(tangent.x) | (packSnorm10(tangent.y) << 10u) | (packSnorm10(tangent.z) << 20u) |
           ((value.w >= 0.0f ? 1u : 3u) << 30u);
}

// textureId -> TextureHandle
using TextureCache = std::vector<lvk::Holder<lvk::TextureHandle>>;
// textureId -> FileName
using TextureFiles = std::vector<std::string>;

inline GLTFMaterialDataGPU convertToGPUMaterial(const std::unique_ptr<lvk::IContext>& ctx, const Material& mat, const TextureFiles& files,
                                                TextureCache& cache)
{
    GLTFMaterialDataGPU result = {
        .baseColorFactor = mat.baseColorFactor,
        .metallicRoughnessNormalOcclusion = vec4(mat.metallicFactor, mat.roughness, 1.0f, 1.0f),
        .clearcoatTransmissionThickness = vec4(1.0f, 1.0f, mat.transparencyFactor, 1.0f),
        .emissiveFactorAlphaCutoff = vec4(vec3(mat.emissiveFactor), mat.alphaTest),
    };

    auto getTextureFromCache = [&cache, &ctx, &files](int textureId) -> uint32_t
    {
        if (textureId == -1)
        {
            return 0;
        }

        if (cache.size() <= textureId)
        {
            cache.resize(textureId + 1);
        }
        if (cache[textureId].empty())
        {
            cache[textureId] = loadTexture(ctx, files[textureId].c_str());
        }
        return cache[textureId].index();
    };

    result.baseColorTexture = getTextureFromCache(mat.baseColorTexture);
    result.emissiveTexture = getTextureFromCache(mat.emissiveTexture);
    result.normalTexture = getTextureFromCache(mat.normalTexture);
    result.transmissionTexture = getTextureFromCache(mat.opacityTexture);

    return result;
}

// NOTE: this function was manually tweaked to load Bistro materials from .obj - use UtilsGLTF.h for anything else
inline Material convertAIMaterial(const aiMaterial* M, std::vector<std::string>& files, std::vector<std::string>& opacityMaps)
{
    Material D;

    aiColor4D Color;

    if (aiGetMaterialColor(M, AI_MATKEY_COLOR_AMBIENT, &Color) == AI_SUCCESS)
    {
        D.emissiveFactor = { Color.r, Color.g, Color.b, Color.a };
        if (D.emissiveFactor.w > 1.0f)
        {
            D.emissiveFactor.w = 1.0f;
        }
    }
    if (aiGetMaterialColor(M, AI_MATKEY_COLOR_DIFFUSE, &Color) == AI_SUCCESS)
    {
        D.baseColorFactor = { Color.r, Color.g, Color.b, Color.a };
        if (D.baseColorFactor.w > 1.0f)
        {
            D.baseColorFactor.w = 1.0f;
        }
    }
    if (aiGetMaterialColor(M, AI_MATKEY_COLOR_EMISSIVE, &Color) == AI_SUCCESS)
    {
        D.emissiveFactor += vec4(Color.r, Color.g, Color.b, Color.a);
        if (D.emissiveFactor.w > 1.0f)
        {
            D.emissiveFactor.w = 1.0f;
        }
    }

    const float opaquenessThreshold = 0.05f;
    float       Opacity = 1.0f;

    if (aiGetMaterialFloat(M, AI_MATKEY_OPACITY, &Opacity) == AI_SUCCESS)
    {
        D.transparencyFactor = glm::clamp(1.0f - Opacity, 0.0f, 1.0f);
        if (D.transparencyFactor >= 1.0f - opaquenessThreshold)
        {
            D.transparencyFactor = 0.0f;
        }
    }

    if (aiGetMaterialColor(M, AI_MATKEY_COLOR_TRANSPARENT, &Color) == AI_SUCCESS)
    {
        const float Opacity = std::max(std::max(Color.r, Color.g), Color.b);
        D.transparencyFactor = glm::clamp(Opacity, 0.0f, 1.0f);
        if (D.transparencyFactor >= 1.0f - opaquenessThreshold)
        {
            D.transparencyFactor = 0.0f;
        }
        D.alphaTest = 0.5f;
    }

    float tmp = 1.0f;
    if (aiGetMaterialFloat(M, AI_MATKEY_METALLIC_FACTOR, &tmp) == AI_SUCCESS)
    {
        D.metallicFactor = tmp;
    }

    if (aiGetMaterialFloat(M, AI_MATKEY_ROUGHNESS_FACTOR, &tmp) == AI_SUCCESS)
    {
        D.roughness = tmp;
    }

    aiString         path;
    aiTextureMapping mapping;
    unsigned int     uvIndex = 0;
    float            blend = 1.0f;
    aiTextureOp      textureOp = aiTextureOp_Add;
    aiTextureMapMode textureMapMode[2] = { aiTextureMapMode_Wrap, aiTextureMapMode_Wrap };
    unsigned int     textureFlags = 0;

    if (aiGetMaterialTexture(M, aiTextureType_EMISSIVE, 0, &path, &mapping, &uvIndex, &blend, &textureOp, textureMapMode, &textureFlags) ==
        AI_SUCCESS)
    {
        D.emissiveTexture = addUnique(files, path.C_Str());
    }

    if (aiGetMaterialTexture(M, aiTextureType_DIFFUSE, 0, &path, &mapping, &uvIndex, &blend, &textureOp, textureMapMode, &textureFlags) ==
        AI_SUCCESS)
    {
        D.baseColorTexture = addUnique(files, path.C_Str());
        const std::string albedoMap = std::string(path.C_Str());
        if (albedoMap.find("grey_30") != albedoMap.npos)
        {
            D.flags |= sMaterialFlags_Transparent;
        }
    }

    // first try tangent space normal map
    if (aiGetMaterialTexture(M, aiTextureType_NORMALS, 0, &path, &mapping, &uvIndex, &blend, &textureOp, textureMapMode, &textureFlags) ==
        AI_SUCCESS)
    {
        D.normalTexture = addUnique(files, path.C_Str());
    }
    // then height map
    if (D.normalTexture == -1)
    {
        if (aiGetMaterialTexture(M, aiTextureType_HEIGHT, 0, &path, &mapping, &uvIndex, &blend, &textureOp, textureMapMode,
                                 &textureFlags) == AI_SUCCESS)
        {
            D.normalTexture = addUnique(files, path.C_Str());
        }
    }

    if (aiGetMaterialTexture(M, aiTextureType_OPACITY, 0, &path, &mapping, &uvIndex, &blend, &textureOp, textureMapMode, &textureFlags) ==
        AI_SUCCESS)
    {
        D.opacityTexture = addUnique(opacityMaps, path.C_Str());
        D.alphaTest = 0.5f;
    }

    // patch materials
    aiString    Name;
    std::string materialName;
    if (aiGetMaterialString(M, AI_MATKEY_NAME, &Name) == AI_SUCCESS)
    {
        materialName = Name.C_Str();
    }
    // apply heuristics
    auto name = [&materialName](const char* substr) -> bool { return materialName.find(substr) != std::string::npos; };
    if (name("MASTER_Glass_Clean") || name("MenuSign_02_Glass") || name("Vespa_Headlight"))
    {
        D.alphaTest = 0.75f;
        D.transparencyFactor = 0.2f;
        D.flags |= sMaterialFlags_Transparent;
    }
    else if (name("MASTER_Glass_Exterior") || name("MASTER_Focus_Glass"))
    {
        D.alphaTest = 0.75f;
        D.transparencyFactor = 0.3f;
        D.flags |= sMaterialFlags_Transparent;
    }
    else if (name("MASTER_Frosted_Glass") || name("MASTER_Interior_01_Frozen_Glass"))
    {
        D.alphaTest = 0.75f;
        D.transparencyFactor = 0.2f;
        D.flags |= sMaterialFlags_Transparent;
    }
    else if (name("Streetlight_Glass"))
    {
        D.alphaTest = 0.75f;
        D.transparencyFactor = 0.15f;
        D.baseColorTexture = -1;
        D.flags |= sMaterialFlags_Transparent;
    }
    else if (name("Paris_LiquorBottle_01_Glass_Wine"))
    {
        D.alphaTest = 0.56f;
        D.transparencyFactor = 0.35f;
        D.flags |= sMaterialFlags_Transparent;
    }
    else if (name("_Caps") || name("_Labels"))
    {
        // not transparent
    }
    else if (name("Paris_LiquorBottle_02_Glass"))
    {
        D.alphaTest = 0.56f;
        D.transparencyFactor = 0.1f;
    }
    else if (name("Bottle"))
    {
        D.alphaTest = 0.56f;
        D.transparencyFactor = 0.2f;
        D.flags |= sMaterialFlags_Transparent;
    }
    else if (name("Glass"))
    {
        D.alphaTest = 0.56f;
        D.transparencyFactor = 0.1f;
        D.flags |= sMaterialFlags_Transparent;
    }
    else if (name("Metal"))
    {
        D.metallicFactor = 1.0f;
        D.roughness = 0.1f;
    }

    return D;
}

inline void processLODs(std::vector<uint32_t>& indices, std::vector<uint8_t>& vertices, size_t vertexStride,
                        std::vector<std::vector<uint32_t>>& outLods, bool generateLods)
{
    size_t verticesCountIn = vertices.size() / vertexStride;
    size_t targetIndicesCount = indices.size();

    LLOGL("   LOD0: %i indices\n", int(indices.size()));

    outLods.push_back(indices);

    if (!generateLods)
    {
        return;
    }

    uint8_t LOD = 1;

    while (targetIndicesCount > 1024 && LOD < kMaxLODs)
    {
        targetIndicesCount /= 2;

        bool sloppy = false;

        size_t numOptIndices = meshopt_simplify(indices.data(), indices.data(), (uint32_t)indices.size(), (const float*)vertices.data(),
                                                verticesCountIn, vertexStride, targetIndicesCount, 0.02f, 0, nullptr);

        // cannot simplify further
        if (static_cast<size_t>(numOptIndices * 1.1f) > indices.size())
        {
            if (LOD > 1)
            {
                // try harder
                numOptIndices = meshopt_simplifySloppy(indices.data(), indices.data(), indices.size(), (const float*)vertices.data(),
                                                       verticesCountIn, vertexStride, targetIndicesCount, 0.02f, nullptr);
                sloppy = true;
                if (numOptIndices == indices.size())
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }

        indices.resize(numOptIndices);

        meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), verticesCountIn);

        LLOGL("   LOD%i: %i indices %s\n", int(LOD), int(numOptIndices), sloppy ? "[sloppy]" : "");

        LOD++;

        outLods.push_back(indices);
    }
}

inline Mesh convertAIMesh(const aiMesh* m, MeshData& meshData, uint32_t& indexOffset, uint32_t& vertexOffset, bool generateLODs)
{
    static_assert(sizeof(aiVector3D) == 3 * sizeof(float));
    static_assert(sizeof(PackedVertexAttribs) == 12);

    const bool hasTexCoords = m->HasTextureCoords(0);
    const bool hasTangents = m->HasTangentsAndBitangents();

    struct SourceVertex
    {
        vec3     pos;
        uint32_t uv;
        uint32_t normal;
        uint32_t tangent;
    };
    static_assert(sizeof(SourceVertex) == sizeof(vec3) + 3 * sizeof(uint32_t));

    // Original data for LOD calculation
    std::vector<uint32_t> srcIndices;
    std::vector<uint8_t>  vertices;

    for (size_t i = 0; i != m->mNumVertices; i++)
    {
        const aiVector3D   v = m->mVertices[i];
        const aiVector3D   n = m->mNormals[i];
        const aiVector2D   t = hasTexCoords ? aiVector2D(m->mTextureCoords[0][i].x, m->mTextureCoords[0][i].y) : aiVector2D();
        const aiVector3D   tangent = hasTangents ? m->mTangents[i] : aiVector3D(1.0f, 0.0f, 0.0f);
        const SourceVertex vertex = {
            .pos = vec3(v.x, v.y, v.z),
            .uv = glm::packHalf2x16(vec2(t.x, t.y)),
            .normal = packSnorm10x3(vec3(n.x, n.y, n.z)),
            .tangent = packTangent10x3Handedness(vec4(tangent.x, tangent.y, tangent.z, 1.0f)),
        };
        put(vertices, vertex);
    }

    // pos, uv, normal
    meshData.streams = {
        .attributes =
            {
                {.location = 0, .binding = 0, .format = lvk::VertexFormat_Float3, .offset = 0},
                {.location = 1,
                 .binding = 1,
                 .format = lvk::VertexFormat_A2B10G10R10_SNorm,
                 .offset = offsetof(PackedVertexAttribs, normal)},
                {.location = 2,
                 .binding = 1,
                 .format = lvk::VertexFormat_UInt1,
                 .offset = offsetof(PackedVertexAttribs, uv)},
                // { .location = 3, .binding = 1, .format = lvk::VertexFormat_A2B10G10R10_SNorm, .offset =
                // offsetof(PackedVertexAttribs, tangent) },
            },
        .inputBindings = {{.stride = sizeof(vec3)}, {.stride = sizeof(PackedVertexAttribs)}},
    };
    meshData.positionOnlyStreams = {
        .attributes = { { .location = 0, .binding = 0, .format = lvk::VertexFormat_Float3, .offset = 0 } },
        .inputBindings = { { .stride = sizeof(vec3) } },
    };

    for (unsigned int i = 0; i != m->mNumFaces; i++)
    {
        if (m->mFaces[i].mNumIndices != 3)
        {
            continue;
        }
        for (unsigned j = 0; j != m->mFaces[i].mNumIndices; j++)
        {
            srcIndices.push_back(m->mFaces[i].mIndices[j]);
        }
    }

    const uint32_t vertexStride = sizeof(SourceVertex);

    // optimize the entire mesh
    {
        const uint32_t        vertexCountIn = vertices.size() / vertexStride;
        std::vector<uint32_t> remap(vertexCountIn);
        const size_t          vertexCountOut =
            meshopt_generateVertexRemap(remap.data(), srcIndices.data(), srcIndices.size(), vertices.data(), vertexCountIn, vertexStride);

        std::vector<uint32_t> remappedIndices(srcIndices.size());
        std::vector<uint8_t>  remappedVertices(vertexCountOut * vertexStride);

        meshopt_remapIndexBuffer(remappedIndices.data(), srcIndices.data(), srcIndices.size(), remap.data());
        meshopt_remapVertexBuffer(remappedVertices.data(), vertices.data(), vertexCountIn, vertexStride, remap.data());

        meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(), srcIndices.size(), vertexCountOut);
        meshopt_optimizeOverdraw(remappedIndices.data(), remappedIndices.data(), srcIndices.size(), (const float*)remappedVertices.data(),
                                 vertexCountOut, vertexStride, 1.05f);
        meshopt_optimizeVertexFetch(remappedVertices.data(), remappedIndices.data(), srcIndices.size(), remappedVertices.data(),
                                    vertexCountOut, vertexStride);

        srcIndices = remappedIndices;
        vertices = remappedVertices;
        LVK_ASSERT(vertexCountOut == vertices.size() / vertexStride);
    }

    const uint32_t numVertices = static_cast<uint32_t>(vertices.size() / vertexStride);

    std::vector<std::vector<uint32_t>> outLods;
    processLODs(srcIndices, vertices, vertexStride, outLods, generateLODs);

    Mesh result = {
        .indexOffset = indexOffset,
        .vertexOffset = vertexOffset,
        .vertexCount = numVertices,
    };

    uint32_t numIndices = 0;
    for (size_t l = 0; l < outLods.size(); l++)
    {
        mergeVectors(meshData.indexData, outLods[l]);
        result.lodOffset[l] = numIndices;
        numIndices += (uint32_t)outLods[l].size();
    }

    std::vector<uint8_t> positions;
    std::vector<uint8_t> attributes;
    positions.reserve(numVertices * sizeof(vec3));
    attributes.reserve(numVertices * sizeof(PackedVertexAttribs));

    for (uint32_t i = 0; i != numVertices; i++)
    {
        const SourceVertex&       vertex = reinterpret_cast<const SourceVertex*>(vertices.data())[i];
        const PackedVertexAttribs attribs = {
            .normal = vertex.normal,
            .uv = vertex.uv,
            .tangent = vertex.tangent,
        };
        put(positions, vertex.pos);
        put(attributes, attribs);
    }

    mergeVectors(meshData.positionData, positions);
    mergeVectors(meshData.attributeData, attributes);

    result.lodOffset[outLods.size()] = numIndices;
    result.lodCount = (uint32_t)outLods.size();
    result.materialID = m->mMaterialIndex;

    indexOffset += numIndices;
    vertexOffset += numVertices;

    return result;
}
