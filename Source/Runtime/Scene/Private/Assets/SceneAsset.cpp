/* Copyright (c) 2026 Horizon */

#include "Scene/SceneAsset.h"
#include "Core/IContainer.h"
#include <ThirdParty/cJSON/cJSON.h>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Core/IMemory.h"

namespace hz
{
class SceneAssetData
{
public:
    AssetID                   id;
    const char*               pSource = "";
    const char*               pContentHash = "";
    Array<char>               strings;
    Array<SceneAssetNode>     nodes;
    Array<SceneAssetMesh>     meshes;
    Array<SceneAssetSubmesh>  submeshes;
    Array<SceneAssetMaterial> materials;
    Array<SceneAssetTexture>  textures;
};

enum class AssetKind
{
    Scene,
    Texture,
    Material,
    Mesh,
};

struct AssetEntry
{
    AssetID   key;
    AssetKind value;
};

class SceneAssetReader
{
public:
    explicit SceneAssetReader(SceneAssetData& data): data(data) {}
    ~SceneAssetReader()
    {
        cJSON_Delete(pRoot);
        hmfree(pAssets);
    }
    SceneAssetReader(const SceneAssetReader&) = delete;
    SceneAssetReader& operator=(const SceneAssetReader&) = delete;

    bool read(Span<char> json);

private:
    bool readTextures(const cJSON* pArray);
    bool readMaterials(const cJSON* pArray);
    bool readMeshes(const cJSON* pArray);
    bool readNodes(const cJSON* pArray);
    bool readID(const cJSON* pObject, AssetID& id, AssetKind kind);
    bool readReference(const cJSON* pObject, const char* pField, AssetID& id, AssetKind kind, bool optional = false);
    bool readString(const cJSON* pObject, const char* pField, const char*& pOutput, bool path = false);

    SceneAssetData& data;
    cJSON*          pRoot = nullptr;
    AssetEntry*     pAssets = nullptr;
};

static bool invalidField(const char* pField)
{
    LOGF(Error, "Invalid cooked SceneAsset field '%s'", pField);
    return false;
}

static const cJSON* field(const cJSON* pObject, const char* Name) { return cJSON_GetObjectItemCaseSensitive(pObject, Name); }

static bool readFloat(const cJSON* pValue, float& output, float minimum = -FLT_MAX, float maximum = FLT_MAX)
{
    if (!cJSON_IsNumber(pValue) || !isfinite(pValue->valuedouble) || pValue->valuedouble < minimum || pValue->valuedouble > maximum)
        return false;
    output = (float)pValue->valuedouble;
    return true;
}

static bool readFloats(const cJSON* pObject, const char* Name, float* pOutput, uint32_t Count, float minimum = -FLT_MAX,
                       float maximum = FLT_MAX)
{
    const cJSON* pArray = field(pObject, Name);
    if (!cJSON_IsArray(pArray) || cJSON_GetArraySize(pArray) != (int)Count)
    {
        return invalidField(Name);
    }
    uint32_t index = 0;
    for (const cJSON* pValue = pArray->child; pValue; pValue = pValue->next)
        if (!readFloat(pValue, pOutput[index++], minimum, maximum))
            return invalidField(Name);
    return true;
}

static bool readInteger(const cJSON* pValue, double minimum, double maximum)
{
    return cJSON_IsNumber(pValue) && isfinite(pValue->valuedouble) && pValue->valuedouble >= minimum && pValue->valuedouble <= maximum &&
           floor(pValue->valuedouble) == pValue->valuedouble;
}

static uint32_t stringSize(const cJSON* pObject, const char* pField)
{
    const cJSON* pValue = field(pObject, pField);
    return cJSON_IsString(pValue) ? (uint32_t)strlen(pValue->valuestring) + 1 : 0;
}

static int compareSubmeshIDs(const void* pA, const void* pB)
{
    const SubmeshID a = *(const SubmeshID*)pA;
    const SubmeshID b = *(const SubmeshID*)pB;
    return (a > b) - (a < b);
}

static bool isRelativePath(const char* pPath)
{
    if (!*pPath || *pPath == '/' || strchr(pPath, '\\') || strchr(pPath, ':'))
        return false;
    const char* pSegment = pPath;
    for (const char* pCursor = pPath;; ++pCursor)
        if (*pCursor == '/' || !*pCursor)
        {
            if (pCursor == pSegment || (pCursor - pSegment == 1 && *pSegment == '.') ||
                (pCursor - pSegment == 2 && pSegment[0] == '.' && pSegment[1] == '.'))
                return false;
            if (!*pCursor)
                return true;
            pSegment = pCursor + 1;
        }
}

bool SceneAssetReader::readString(const cJSON* pObject, const char* pField, const char*& pOutput, bool path)
{
    const cJSON* pValue = field(pObject, pField);
    if (!cJSON_IsString(pValue) || (path && !isRelativePath(pValue->valuestring)))
        return invalidField(pField);
    const uint32_t offset = data.strings.Size();
    const uint32_t Size = (uint32_t)strlen(pValue->valuestring) + 1;
    data.strings.Resize(offset + Size);
    memcpy(data.strings.GetData() + offset, pValue->valuestring, Size);
    pOutput = data.strings.GetData() + offset;
    return true;
}

bool SceneAssetReader::readID(const cJSON* pObject, AssetID& id, AssetKind kind)
{
    const cJSON* pValue = field(pObject, "id");
    if (!cJSON_IsString(pValue) || !id.Parse(pValue->valuestring) || !id.IsValid() || hmgetp_null(pAssets, id))
    {
        return invalidField("id (invalid or duplicate AssetID)");
    }
    const AssetEntry entry = { .key = id, .value = kind };
    hmputs(pAssets, entry);
    return true;
}

bool SceneAssetReader::readReference(const cJSON* pObject, const char* pField, AssetID& id, AssetKind kind, bool optional)
{
    const cJSON* pValue = field(pObject, pField);
    if (cJSON_IsNull(pValue) || (!pValue && optional))
        return true;
    if (!cJSON_IsString(pValue) || !id.Parse(pValue->valuestring) || !id.IsValid())
    {
        return invalidField(pField);
    }
    const AssetEntry* pEntry = hmgetp_null(pAssets, id);
    return (pEntry && pEntry->value == kind) || invalidField(pField);
}

bool SceneAssetReader::readTextures(const cJSON* pArray)
{
    if (!cJSON_IsArray(pArray))
        return invalidField("textures");
    data.textures.Reserve((uint32_t)cJSON_GetArraySize(pArray));
    for (const cJSON* pItem = pArray->child; pItem; pItem = pItem->next)
    {
        SceneAssetTexture texture;
        if (!readID(pItem, texture.id, AssetKind::Texture) || !readString(pItem, "path", texture.pPath, true) ||
            !readString(pItem, "source", texture.pSource, true))
            return false;
        const cJSON* pSrgb = field(pItem, "srgb");
        if (!cJSON_IsBool(pSrgb))
            return invalidField("srgb");
        texture.srgb = cJSON_IsTrue(pSrgb);
        const cJSON* pCooked = field(pItem, "cooked");
        if (!cJSON_IsBool(pCooked))
            return invalidField("cooked");
        texture.cooked = cJSON_IsTrue(pCooked);
        data.textures.PushBack(texture);
    }
    return true;
}

bool SceneAssetReader::readMaterials(const cJSON* pArray)
{
    if (!cJSON_IsArray(pArray))
        return invalidField("materials");
    data.materials.Reserve((uint32_t)cJSON_GetArraySize(pArray));
    for (const cJSON* pItem = pArray->child; pItem; pItem = pItem->next)
    {
        SceneAssetMaterial material;
        if (!readID(pItem, material.id, AssetKind::Material) || !readString(pItem, "name", material.Name) ||
            !readReference(pItem, "baseColorTexture", material.baseColorTexture, AssetKind::Texture) ||
            !readReference(pItem, "normalTexture", material.normalTexture, AssetKind::Texture) ||
            !readReference(pItem, "metallicRoughnessTexture", material.metallicRoughnessTexture, AssetKind::Texture) ||
            !readReference(pItem, "emissiveTexture", material.emissiveTexture, AssetKind::Texture) ||
            !readFloats(pItem, "baseColorFactor", material.baseColorFactor, 4, 0.0f, 1.0f) ||
            !readFloats(pItem, "emissiveFactor", material.emissiveFactor, 3, 0.0f))
        {
            return false;
        }
        if (!readFloat(field(pItem, "metallicFactor"), material.metallicFactor, 0.0f, 1.0f) ||
            !readFloat(field(pItem, "roughnessFactor"), material.roughnessFactor, 0.0f, 1.0f) ||
            !readFloat(field(pItem, "alphaCutoff"), material.alphaCutoff, 0.0f) || !readInteger(field(pItem, "alphaMode"), 0, 2) ||
            !cJSON_IsBool(field(pItem, "doubleSided")))
            return invalidField("material factors/alpha mode");
        material.alphaMode = (MaterialAlphaMode)field(pItem, "alphaMode")->valueint;
        material.doubleSided = cJSON_IsTrue(field(pItem, "doubleSided"));
        // Optional, so assets cooked before tags existed stay valid.
        const cJSON* pTags = field(pItem, "tags");
        if (pTags && !readInteger(pTags, 0, UINT32_MAX))
        {
            return invalidField("material tags");
        }
        material.tags = pTags ? (uint32_t)pTags->valuedouble : 0;
        data.materials.PushBack(material);
    }
    return true;
}

bool SceneAssetReader::readMeshes(const cJSON* pArray)
{
    if (!cJSON_IsArray(pArray))
        return invalidField("meshes");
    uint32_t submeshCount = 0;
    for (const cJSON* pItem = pArray->child; pItem; pItem = pItem->next)
    {
        if (!cJSON_IsArray(field(pItem, "submeshes")) || !field(pItem, "submeshes")->child)
            return invalidField("submeshes");
        submeshCount += (uint32_t)cJSON_GetArraySize(field(pItem, "submeshes"));
    }
    data.submeshes.Reserve(submeshCount);
    data.meshes.Reserve((uint32_t)cJSON_GetArraySize(pArray));
    Array<SubmeshID> ids;
    for (const cJSON* pItem = pArray->child; pItem; pItem = pItem->next)
    {
        SceneAssetMesh mesh;
        if (!readID(pItem, mesh.id, AssetKind::Mesh) || !readString(pItem, "name", mesh.Name) ||
            !readString(pItem, "geometry", mesh.pGeometry, true))
        {
            return false;
        }
        const uint32_t first = data.submeshes.Size();
        ids.Clear();
        for (const cJSON* pSubmesh = field(pItem, "submeshes")->child; pSubmesh; pSubmesh = pSubmesh->next)
        {
            const cJSON* pID = field(pSubmesh, "id");
            if (!cJSON_IsString(pID) || strlen(pID->valuestring) != 16 || strspn(pID->valuestring, "0123456789abcdefABCDEF") != 16 ||
                !readInteger(field(pSubmesh, "draw"), 0, UINT32_MAX))
                return invalidField("submesh id/Draw");
            SceneAssetSubmesh submesh = { .id = strtoull(pID->valuestring, nullptr, 16),
                                          .Draw = (uint32_t)field(pSubmesh, "draw")->valuedouble };
            if (!submesh.id)
                return invalidField("submesh id");
            if (!readReference(pSubmesh, "material", submesh.material, AssetKind::Material))
                return false;
            if (!submesh.material.IsValid())
            {
                return invalidField("submesh material");
            }
            data.submeshes.PushBack(submesh);
            ids.PushBack(submesh.id);
        }
        qsort(ids.GetData(), ids.Size(), sizeof(SubmeshID), compareSubmeshIDs);
        for (uint32_t i = 1; i < ids.Size(); ++i)
        {
            if (ids[i - 1] == ids[i])
                return invalidField("submeshes (duplicate SubmeshID within mesh)");
        }
        mesh.submeshes = { data.submeshes.GetData() + first, data.submeshes.Size() - first };
        data.meshes.PushBack(mesh);
    }
    return true;
}

static bool validateHierarchy(Span<SceneAssetNode> nodes)
{
    Array<uint8_t> visited(nodes.Count);
    for (uint32_t i = 0; i < nodes.Count; ++i)
    {
        uint32_t cursor = i;
        while (cursor != UINT32_MAX && visited[cursor] == 0)
        {
            visited[cursor] = 1;
            cursor = nodes.Data[cursor].parent;
        }
        if (cursor != UINT32_MAX && visited[cursor] == 1)
            return invalidField("nodes.parent (hierarchy cycle)");
        cursor = i;
        while (cursor != UINT32_MAX && visited[cursor] == 1)
        {
            visited[cursor] = 2;
            cursor = nodes.Data[cursor].parent;
        }
    }
    return true;
}

bool SceneAssetReader::readNodes(const cJSON* pArray)
{
    if (!cJSON_IsArray(pArray))
        return invalidField("nodes");
    const uint32_t Count = (uint32_t)cJSON_GetArraySize(pArray);
    data.nodes.Reserve(Count);
    for (const cJSON* pItem = pArray->child; pItem; pItem = pItem->next)
    {
        SceneAssetNode node;
        if (!readString(pItem, "name", node.Name) || !readReference(pItem, "mesh", node.mesh, AssetKind::Mesh, true))
        {
            return false;
        }
        if (!readInteger(field(pItem, "parent"), -1, (double)Count - 1))
        {
            return invalidField("nodes.parent");
        }
        const double parent = field(pItem, "parent")->valuedouble;
        node.parent = parent < 0 ? UINT32_MAX : (uint32_t)parent;
        node.hasMatrix = field(pItem, "matrix") != nullptr;
        if (node.hasMatrix)
        {
            float matrix[16];
            if (field(pItem, "translation") || field(pItem, "rotation") || field(pItem, "scale"))
                return invalidField("nodes (matrix and TRS are mutually exclusive)");
            if (!readFloats(pItem, "matrix", matrix, 16))
                return false;
            if (matrix[3] != 0.0f || matrix[7] != 0.0f || matrix[11] != 0.0f || matrix[15] != 1.0f)
                return invalidField("nodes.matrix (expected affine matrix)");
            for (uint32_t column = 0; column < 4; ++column)
                for (uint32_t row = 0; row < 4; ++row)
                    node.matrix.setElem((int)column, (int)row, matrix[column * 4 + row]);
        }
        else
        {
            float translation[3], rotation[4], scale[3];
            if (!readFloats(pItem, "translation", translation, 3) || !readFloats(pItem, "rotation", rotation, 4) ||
                !readFloats(pItem, "scale", scale, 3))
                return false;
            const double length = (double)rotation[0] * rotation[0] + (double)rotation[1] * rotation[1] +
                                  (double)rotation[2] * rotation[2] + (double)rotation[3] * rotation[3];
            if (fabs(length - 1.0) > 0.001)
                return invalidField("nodes.rotation (expected unit quaternion)");
            node.transform.translation = Vector3(translation[0], translation[1], translation[2]);
            node.transform.rotation = Quat(rotation[0], rotation[1], rotation[2], rotation[3]);
            node.transform.scale = Vector3(scale[0], scale[1], scale[2]);
        }
        data.nodes.PushBack(node);
    }
    return validateHierarchy({ data.nodes.GetData(), data.nodes.Size() });
}

bool SceneAssetReader::read(Span<char> json)
{
    const char* pEnd = nullptr;
    if (json.Data && json.Count && json.Count <= 64 * 1024 * 1024)
    {
        pRoot = cJSON_ParseWithLengthOpts(json.Data, json.Count, &pEnd, false);
    }
    if (pRoot)
    {
        while (pEnd < json.Data + json.Count && isspace((unsigned char)*pEnd))
        {
            ++pEnd;
        }
        // Accept both byte spans and string literals, which include their final null.
        if (pEnd < json.Data + json.Count && !*pEnd && pEnd + 1 == json.Data + json.Count)
        {
            ++pEnd;
        }
    }
    if (!cJSON_IsObject(pRoot) || pEnd != json.Data + json.Count)
    {
        return invalidField("JSON (expected one object, at most 64 MiB)");
    }
    const cJSON* pFormat = field(pRoot, "format");
    const cJSON* pVersion = field(pRoot, "version");
    if (!cJSON_IsString(pFormat) || strcmp(pFormat->valuestring, "Horizon.SceneAsset") != 0 || !readInteger(pVersion, 1, 1))
        return invalidField("format/version");
    uint32_t    stringBytes = stringSize(pRoot, "source") + stringSize(pRoot, "contentHash");
    const char* tables[] = { "textures", "materials", "meshes", "nodes" };
    for (const char* pTable : tables)
        for (const cJSON* pItem = field(pRoot, pTable) ? field(pRoot, pTable)->child : nullptr; pItem; pItem = pItem->next)
            stringBytes +=
                stringSize(pItem, "name") + stringSize(pItem, "path") + stringSize(pItem, "geometry") + stringSize(pItem, "source");
    data.strings.Reserve(stringBytes);
    if (!readID(pRoot, data.id, AssetKind::Scene) || !readString(pRoot, "source", data.pSource, true) ||
        !readString(pRoot, "contentHash", data.pContentHash))
        return false;
    const cJSON* pIdentityHash = field(pRoot, "identityHash");
    if (strncmp(data.pContentHash, "fnv1a64:", 8) != 0 || strlen(data.pContentHash) != 24 ||
        strspn(data.pContentHash + 8, "0123456789abcdefABCDEF") != 16 || !cJSON_IsString(pIdentityHash) ||
        strlen(pIdentityHash->valuestring) != 16 || strspn(pIdentityHash->valuestring, "0123456789abcdefABCDEF") != 16)
        return invalidField("contentHash/identityHash");
    return readTextures(field(pRoot, "textures")) && readMaterials(field(pRoot, "materials")) && readMeshes(field(pRoot, "meshes")) &&
           readNodes(field(pRoot, "nodes"));
}

SceneAsset::~SceneAsset() { MemDelete(Data); }

SceneAsset::SceneAsset(SceneAsset&& other) noexcept: Data(other.Data) { other.Data = nullptr; }

SceneAsset& SceneAsset::operator=(SceneAsset&& other) noexcept
{
    if (this != &other)
    {
        MemDelete(Data);
        Data = other.Data;
        other.Data = nullptr;
    }
    return *this;
}

bool SceneAsset::Parse(Span<char> json)
{
    SceneAssetData*  pCandidate = MemNew(SceneAssetData);
    SceneAssetReader reader(*pCandidate);
    if (!reader.read(json))
    {
        MemDelete(pCandidate);
        return false;
    }
    MemDelete(Data);
    Data = pCandidate;
    return true;
}

AssetID              SceneAsset::GetID() const { return Data ? Data->id : AssetID{}; }
const char*          SceneAsset::GetSource() const { return Data ? Data->pSource : ""; }
const char*          SceneAsset::GetContentHash() const { return Data ? Data->pContentHash : ""; }
Span<SceneAssetNode> SceneAsset::GetNodes() const
{
    return Data ? Span<SceneAssetNode>(Data->nodes.GetData(), Data->nodes.Size()) : Span<SceneAssetNode>{};
}
Span<SceneAssetMesh> SceneAsset::GetMeshes() const
{
    return Data ? Span<SceneAssetMesh>(Data->meshes.GetData(), Data->meshes.Size()) : Span<SceneAssetMesh>{};
}
Span<SceneAssetMaterial> SceneAsset::GetMaterials() const
{
    return Data ? Span<SceneAssetMaterial>(Data->materials.GetData(), Data->materials.Size()) : Span<SceneAssetMaterial>{};
}
Span<SceneAssetTexture> SceneAsset::GetTextures() const
{
    return Data ? Span<SceneAssetTexture>(Data->textures.GetData(), Data->textures.Size()) : Span<SceneAssetTexture>{};
}
static void copyName(Name& name, const char* pText)
{
    size_t length = 0;
    while (length < sizeof(name.value) - 1 && pText[length])
        ++length;
    while (length && ((uint8_t)pText[length] & 0xc0) == 0x80)
        --length;
    memcpy(name.value, pText, length);
    name.value[length] = 0;
}

Entity SceneAsset::instantiate(World& world, Entity parent) const
{
    ASSERT(Data && !world.IsDeferred() && (!parent.IsValid() || world.IsAlive(parent)));
    const Entity root = world.CreatePersistentEntity();
    if (!root.IsValid())
    {
        return {};
    }
    Array<Entity> entities;
    entities.Reserve(Data->nodes.Size());
    {
        DeferredChanges changes = world.Defer();
        Name            name;
        copyName(name, Data->pSource);
        bool copied = world.SetComponent(root, SceneTypes::name, &name);
        for (const SceneAssetNode& node : Data->nodes)
        {
            const Entity entity = world.CreatePersistentEntity();
            if (!entity.IsValid())
            {
                break;
            }
            entities.PushBack(entity);
            copyName(name, node.Name);
            copied &= world.SetComponent(entity, SceneTypes::name, &name);
            copied &= node.hasMatrix ? world.SetComponent(entity, SceneTypes::localMatrix, &node.matrix)
                                     : world.SetComponent(entity, SceneTypes::localTransform, &node.transform);
            if (node.mesh.IsValid())
            {
                const MeshRenderer renderer = { .mesh = node.mesh };
                copied &= world.SetComponent(entity, SceneTypes::meshRenderer, &renderer);
            }
        }
        ASSERT(copied);
    }
    if (entities.Size() != Data->nodes.Size())
    {
        for (Entity entity : entities)
            world.DestroyEntity(entity);
        world.DestroyEntity(root);
        return {};
    }
    if (parent.IsValid())
    {
        world.AttachParent(root, parent);
    }
    for (uint32_t i = 0; i < entities.Size(); ++i)
    {
        world.AttachParent(entities[i], Data->nodes[i].parent == UINT32_MAX ? root : entities[Data->nodes[i].parent]);
    }
    return root;
}
} // namespace hz
