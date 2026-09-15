/* Copyright (c) 2026 Horizon */

#include "SceneAssetCooker.h"
#include "Core/IContainer.h"
#include "Core/IToolFileSystem.h"
#include "Scene/SceneID.h"
#include <ThirdParty/cJSON/cJSON.h>
#include <ThirdParty/cgltf/cgltf.h>
#include <stdlib.h>
#include <string.h>

class JsonDocument
{
public:
    explicit JsonDocument(cJSON* pValue = nullptr): pValue(pValue) {}
    ~JsonDocument() { cJSON_Delete(pValue); }
    JsonDocument(const JsonDocument&) = delete;
    JsonDocument& operator=(const JsonDocument&) = delete;
    cJSON*        get() const { return pValue; }

private:
    cJSON* pValue;
};

#include "Core/IMemory.h"

static const char* text(const cJSON* pObject, const char* pField)
{
    const cJSON* pValue = cJSON_GetObjectItemCaseSensitive(pObject, pField);
    return cJSON_IsString(pValue) ? pValue->valuestring : "";
}

static uint64_t hashBytes(const void* pData, size_t size, uint64_t hash = UINT64_C(14695981039346656037))
{
    const uint8_t* pBytes = (const uint8_t*)pData;
    for (size_t i = 0; i < size; ++i)
        hash = (hash ^ pBytes[i]) * UINT64_C(1099511628211);
    return hash;
}

static void addHash(cJSON* pObject, const char* pField, uint64_t hash)
{
    char value[17];
    snprintf(value, sizeof(value), "%016llx", (unsigned long long)hash);
    cJSON_AddStringToObject(pObject, pField, value);
}

static cJSON* readJson(ResourceDirectory directory, const char* pPath, uint64_t* pHash = nullptr)
{
    FileStream stream = {};
    if (!fsOpenStreamFromPath(directory, pPath, FM_READ, &stream))
        return nullptr;
    const ssize_t size = fsGetStreamFileSize(&stream);
    if (size <= 0 || size > 64 * 1024 * 1024)
    {
        fsCloseStream(&stream);
        return nullptr;
    }
    hz::Array<char> bytes((uint32_t)size + 1);
    const bool      read = fsReadFromStream(&stream, bytes.data(), (size_t)size) == (size_t)size;
    fsCloseStream(&stream);
    if (!read)
        return nullptr;
    if (pHash)
        *pHash = hashBytes(bytes.data(), (size_t)size);
    return cJSON_ParseWithLength(bytes.data(), bytes.size());
}

static bool writeJson(ResourceDirectory directory, const char* pPath, const cJSON* pValue, uint64_t* pHash = nullptr)
{
    char* pText = cJSON_Print(pValue);
    if (!pText)
        return false;
    char         temporary[FS_MAX_PATH];
    const bool   validPath = snprintf(temporary, sizeof(temporary), "%s.tmp", pPath) < (int)sizeof(temporary);
    FileStream   stream = {};
    const size_t size = strlen(pText);
    const bool   opened = validPath && fsOpenStreamFromPath(directory, temporary, FM_WRITE, &stream);
    const bool   written = opened && fsWriteToStream(&stream, pText, size) == size;
    const bool   closed = !opened || fsCloseStream(&stream);
    if (pHash)
        *pHash = hashBytes(pText, size);
    cJSON_free(pText);
    if (!written || !closed)
    {
        LOGF(eERROR, "Cannot publish scene asset file '%s'", pPath);
        return false;
    }
    char backup[FS_MAX_PATH];
    if (snprintf(backup, sizeof(backup), "%s.bak", pPath) >= (int)sizeof(backup))
        return false;
    const bool exists = fsFileExist(directory, pPath);
    if (fsFileExist(directory, backup) || (exists && !fsRenameFile(directory, pPath, backup)))
    {
        LOGF(eERROR, "Cannot back up '%s'; check for an interrupted cook", pPath);
        return false;
    }
    if (!fsRenameFile(directory, temporary, pPath))
    {
        if (exists && !fsRenameFile(directory, backup, pPath))
            LOGF(eERROR, "Restore '%s' from '%s' before retrying", pPath, backup);
        return false;
    }
    if (exists && !fsRemoveFile(directory, backup))
        LOGF(eWARNING, "Cannot remove completed cook backup '%s'", backup);
    return true;
}

static void addFloats(cJSON* pObject, const char* pField, const float* pValues, uint32_t count)
{
    cJSON* pArray = cJSON_AddArrayToObject(pObject, pField);
    for (uint32_t i = 0; i < count; ++i)
        cJSON_AddItemToArray(pArray, cJSON_CreateNumber(pValues[i]));
}

static void addSourceKey(cJSON* pObject, const cgltf_data& data, const cgltf_extras& extras)
{
    if (extras.end_offset <= extras.start_offset)
        return;
    const JsonDocument json(cJSON_ParseWithLength(data.json + extras.start_offset, extras.end_offset - extras.start_offset));
    const char*        pKey = text(json.get(), "horizonId");
    if (*pKey)
        cJSON_AddStringToObject(pObject, "sourceKey", pKey);
}

static cJSON* identity(const char* pKind, const char* pName, const cgltf_data& data, const cgltf_extras& extras)
{
    cJSON* pRecord = cJSON_CreateObject();
    cJSON_AddStringToObject(pRecord, "kind", pKind);
    cJSON_AddStringToObject(pRecord, "name", pName ? pName : "");
    addSourceKey(pRecord, data, extras);
    cJSON_AddBoolToObject(pRecord, "missing", false);
    return pRecord;
}

static bool sameField(const cJSON* pA, const cJSON* pB, const char* pField)
{
    const char* pValue = text(pA, pField);
    return *pValue && strcmp(text(pA, "kind"), text(pB, "kind")) == 0 && strcmp(pValue, text(pB, pField)) == 0;
}

static cJSON* uniqueMatch(cJSON* pOld, cJSON* pNew, cJSON* pRecord, const char* pField)
{
    uint32_t count = 0;
    cJSON*   pCandidate = nullptr;
    for (cJSON* pItem = pOld ? pOld->child : nullptr; pItem; pItem = pItem->next)
        if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(pItem, "missing")) && sameField(pRecord, pItem, pField))
        {
            pCandidate = pItem;
            ++count;
        }
    if (count != 1)
        return nullptr;
    count = 0;
    for (cJSON* pItem = pNew->child; pItem; pItem = pItem->next)
        if (sameField(pRecord, pItem, pField))
            ++count;
    return count == 1 ? pCandidate : nullptr;
}

static bool containsID(const cJSON* pRecords, const char* pID)
{
    for (const cJSON* pItem = pRecords ? pRecords->child : nullptr; pItem; pItem = pItem->next)
        if (strcmp(text(pItem, "id"), pID) == 0)
            return true;
    return false;
}

static bool matchIdentities(cJSON* pOld, cJSON* pNew, bool submeshes = false)
{
    hz::Array<cJSON*> matches((uint32_t)cJSON_GetArraySize(pNew));
    uint32_t          index = 0;
    for (cJSON* pRecord = pNew->child; pRecord; pRecord = pRecord->next, ++index)
    {
        if (*text(pRecord, "sourceKey"))
            matches[index] = uniqueMatch(pOld, pNew, pRecord, "sourceKey");
        else
        {
            cJSON* pName = uniqueMatch(pOld, pNew, pRecord, "name");
            cJSON* pSignature = uniqueMatch(pOld, pNew, pRecord, "signature");
            matches[index] = pName && pSignature && pName != pSignature ? nullptr : (pSignature ? pSignature : pName);
            if (matches[index] && *text(matches[index], "sourceKey"))
                matches[index] = nullptr;
        }
    }
    index = 0;
    for (cJSON* pRecord = pNew->child; pRecord; pRecord = pRecord->next, ++index)
    {
        cJSON* pMatch = matches[index];
        for (uint32_t i = 0; pMatch && i < matches.size(); ++i)
            if (i != index && matches[i] == pMatch)
                pMatch = nullptr;
        char id[hz::kIDStringCapacity];
        if (pMatch)
            snprintf(id, sizeof(id), "%s", text(pMatch, "id"));
        else
        {
            const hz::AssetID generated = hz::AssetID::create();
            if (!generated.isValid())
                return false;
            if (submeshes)
                snprintf(id, sizeof(id), "%016llx", (unsigned long long)generated.low);
            else
                generated.toString(id);
            if (containsID(pOld, id) || containsID(pNew, id))
            {
                LOGF(eERROR, "Generated scene asset identity collision");
                return false;
            }
            if (pOld && pOld->child)
                LOGF(eWARNING, "No unique identity match for %s '%s'; assigning a new ID", text(pRecord, "kind"), text(pRecord, "name"));
        }
        cJSON_AddStringToObject(pRecord, "id", id);
        cJSON* pSubmeshes = cJSON_GetObjectItemCaseSensitive(pRecord, "submeshes");
        if (pSubmeshes && !matchIdentities(cJSON_GetObjectItemCaseSensitive(pMatch, "submeshes"), pSubmeshes, true))
            return false;
    }
    for (const cJSON* pRecord = pOld ? pOld->child : nullptr; pRecord; pRecord = pRecord->next)
        if (!containsID(pNew, text(pRecord, "id")))
        {
            cJSON* pMissing = cJSON_Duplicate(pRecord, true);
            cJSON_ReplaceItemInObjectCaseSensitive(pMissing, "missing", cJSON_CreateBool(true));
            cJSON_AddItemToArray(pNew, pMissing);
        }
    return true;
}

static bool validateIdentities(const cJSON* pRecords, bool submeshes = false)
{
    if (!cJSON_IsArray(pRecords))
        return false;
    for (const cJSON* pRecord = pRecords->child; pRecord; pRecord = pRecord->next)
    {
        const char* pID = text(pRecord, "id");
        hz::AssetID id;
        if (submeshes)
        {
            if (strlen(pID) != 16 || strspn(pID, "0123456789abcdef") != 16 || strtoull(pID, nullptr, 16) == 0)
                return false;
        }
        else if (!id.parse(pID) || !id.isValid())
            return false;
        if (!*text(pRecord, "kind") || !cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(pRecord, "missing")))
            return false;
        for (const cJSON* pOther = pRecord->next; pOther; pOther = pOther->next)
            if (strcmp(pID, text(pOther, "id")) == 0)
                return false;
        const cJSON* pSubmeshes = cJSON_GetObjectItemCaseSensitive(pRecord, "submeshes");
        if (pSubmeshes && (submeshes || !validateIdentities(pSubmeshes, true)))
            return false;
    }
    return true;
}

static uint64_t accessorHash(const cgltf_accessor* pAccessor)
{
    if (!pAccessor)
        return 0;
    uint64_t hash = hashBytes(&pAccessor->type, sizeof(pAccessor->type));
    hash = hashBytes(&pAccessor->count, sizeof(pAccessor->count), hash);
    if (pAccessor->is_sparse)
    {
        hz::Array<float> values((uint32_t)(pAccessor->count * cgltf_num_components(pAccessor->type)));
        if (cgltf_accessor_unpack_floats(pAccessor, values.data(), values.size()) != values.size())
            return 0;
        return hashBytes(values.data(), values.size() * sizeof(float), hash);
    }
    for (cgltf_size i = 0; i < pAccessor->count; ++i)
    {
        float values[16] = {};
        if (!cgltf_accessor_read_float(pAccessor, i, values, 16))
            return 0;
        hash = hashBytes(values, cgltf_num_components(pAccessor->type) * sizeof(float), hash);
    }
    return hash;
}

static int compareHashes(const void* pA, const void* pB)
{
    const uint64_t a = *(const uint64_t*)pA;
    const uint64_t b = *(const uint64_t*)pB;
    return (a > b) - (a < b);
}

static uint64_t primitiveHash(const cgltf_primitive& primitive)
{
    hz::Array<uint64_t> attributes((uint32_t)primitive.attributes_count);
    for (uint32_t i = 0; i < attributes.size(); ++i)
    {
        const cgltf_attribute& attribute = primitive.attributes[i];
        attributes[i] = accessorHash(attribute.data);
        attributes[i] = hashBytes(attribute.name, strlen(attribute.name), attributes[i]);
    }
    qsort(attributes.data(), attributes.size(), sizeof(uint64_t), compareHashes);
    uint64_t hash = hashBytes(attributes.data(), attributes.size() * sizeof(uint64_t));
    hash = hashBytes(&primitive.type, sizeof(primitive.type), hash);
    if (primitive.indices)
        for (cgltf_size i = 0; i < primitive.indices->count; ++i)
        {
            const uint64_t value = cgltf_accessor_read_index(primitive.indices, i);
            hash = hashBytes(&value, sizeof(value), hash);
        }
    return hash;
}

SceneAssetCooker::SceneAssetCooker(ResourceDirectory sourceDirectory, const char* pSourceFile, ResourceDirectory outputDirectory,
                                   const char* pGeometryFile):
    sourceDirectory(sourceDirectory), outputDirectory(outputDirectory), pSourceFile(pSourceFile), pGeometryFile(pGeometryFile)
{
    fsAppendPathExtension(pSourceFile, "asset.json", metadataFile);
    fsReplacePathExtension(pGeometryFile, "sceneasset.json", assetFile);
}

SceneAssetCooker::~SceneAssetCooker()
{
    cJSON_Delete(pMetadata);
    cJSON_Delete(pPrevious);
    cJSON_Delete(pAsset);
}

bool SceneAssetCooker::isCurrent(const char* pContentHash) const
{
    if (!fsFileExist(sourceDirectory, metadataFile) || !fsFileExist(outputDirectory, assetFile))
        return false;
    uint64_t           hash = 0;
    const JsonDocument metadata(readJson(sourceDirectory, metadataFile, &hash));
    const JsonDocument asset(readJson(outputDirectory, assetFile));
    char               value[17];
    snprintf(value, sizeof(value), "%016llx", (unsigned long long)hash);
    const cJSON* pVersion = cJSON_GetObjectItemCaseSensitive(asset.get(), "version");
    return metadata.get() && cJSON_IsNumber(pVersion) && pVersion->valuedouble == 1 &&
           strcmp(text(asset.get(), "format"), "Horizon.SceneAsset") == 0 && strcmp(text(asset.get(), "contentHash"), pContentHash) == 0 &&
           strcmp(text(asset.get(), "identityHash"), value) == 0;
}

bool SceneAssetCooker::prepare()
{
    ASSERT(!pMetadata && !pAsset);
    const bool exists = fsFileExist(sourceDirectory, metadataFile);
    pPrevious = exists ? readJson(sourceDirectory, metadataFile) : nullptr;
    const cJSON* pVersion = cJSON_GetObjectItemCaseSensitive(pPrevious, "version");
    cJSON*       pOldRecords = cJSON_GetObjectItemCaseSensitive(pPrevious, "assets");
    hz::AssetID  sceneID;
    if (exists)
    {
        if (!cJSON_IsNumber(pVersion) || pVersion->valuedouble != 1 || !sceneID.parse(text(pPrevious, "scene")) || !sceneID.isValid() ||
            !validateIdentities(pOldRecords) || containsID(pOldRecords, text(pPrevious, "scene")))
        {
            LOGF(eERROR, "Invalid scene identity sidecar '%s'; refusing to replace persistent IDs", metadataFile);
            return false;
        }
    }
    else
    {
        if (fsFileExist(outputDirectory, assetFile))
        {
            LOGF(eERROR, "Missing identity sidecar '%s'; restore it before recooking", metadataFile);
            return false;
        }
        sceneID = hz::AssetID::create();
        if (!sceneID.isValid())
            return false;
    }
    pMetadata = cJSON_CreateObject();
    cJSON_AddNumberToObject(pMetadata, "version", 1);
    char id[hz::kIDStringCapacity];
    sceneID.toString(id);
    cJSON_AddStringToObject(pMetadata, "scene", id);
    return true;
}

bool SceneAssetCooker::build(const cgltf_data& data, const cJSON& legacyManifest)
{
    ASSERT(pMetadata && !pAsset);
    cJSON*            pOldRecords = cJSON_GetObjectItemCaseSensitive(pPrevious, "assets");
    cJSON*            pRecords = cJSON_AddArrayToObject(pMetadata, "assets");
    const cJSON*      pLegacyScene = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(&legacyManifest, "scenes"),
                                                                      text(&legacyManifest, "defaultScene"));
    const cJSON*      pLegacyMaterials = cJSON_GetObjectItemCaseSensitive(pLegacyScene, "materials");
    const cJSON*      pLegacyTextures = cJSON_GetObjectItemCaseSensitive(pLegacyScene, "textures");
    hz::Array<cJSON*> textures((uint32_t)data.textures_count);
    hz::Array<cJSON*> materials((uint32_t)data.materials_count);
    hz::Array<cJSON*> meshes((uint32_t)data.meshes_count);
    for (uint32_t i = 0; i < textures.size(); ++i)
    {
        const cgltf_texture& texture = data.textures[i];
        textures[i] = identity("Texture", texture.name ? texture.name : texture.image->uri, data, texture.extras);
        addHash(textures[i], "signature", hashBytes(texture.image->uri, strlen(texture.image->uri)));
        cJSON_AddItemToArray(pRecords, textures[i]);
    }
    for (uint32_t i = 0; i < materials.size(); ++i)
    {
        const cgltf_material& material = data.materials[i];
        materials[i] = identity("Material", material.name, data, material.extras);
        // Texture array indices are not identity evidence.
        JsonDocument signature(cJSON_Duplicate(cJSON_GetArrayItem(pLegacyMaterials, (int)i), true));
        cJSON_DeleteItemFromObjectCaseSensitive(signature.get(), "name");
        const char* fields[] = { "baseColorTexture", "normalTexture", "metallicRoughnessTexture", "emissiveTexture" };
        for (const char* pField : fields)
        {
            const int index = cJSON_GetObjectItemCaseSensitive(signature.get(), pField)->valueint;
            cJSON_ReplaceItemInObjectCaseSensitive(
                signature.get(), pField, cJSON_CreateString(index >= 0 ? text(cJSON_GetArrayItem(pLegacyTextures, index), "path") : ""));
        }
        cJSON_AddNumberToObject(signature.get(), "alphaMode", material.alpha_mode);
        cJSON_AddNumberToObject(signature.get(), "alphaCutoff", material.alpha_cutoff);
        cJSON_AddBoolToObject(signature.get(), "doubleSided", material.double_sided);
        char* pText = cJSON_PrintUnformatted(signature.get());
        addHash(materials[i], "signature", hashBytes(pText, strlen(pText)));
        cJSON_free(pText);
        cJSON_AddItemToArray(pRecords, materials[i]);
    }
    for (uint32_t i = 0; i < meshes.size(); ++i)
    {
        const cgltf_mesh& mesh = data.meshes[i];
        meshes[i] = identity("Mesh", mesh.name, data, mesh.extras);
        cJSON*              pSubmeshes = cJSON_AddArrayToObject(meshes[i], "submeshes");
        hz::Array<uint64_t> hashes((uint32_t)mesh.primitives_count);
        for (uint32_t j = 0; j < hashes.size(); ++j)
        {
            hashes[j] = primitiveHash(mesh.primitives[j]);
            cJSON* pSubmesh = identity("Submesh", nullptr, data, mesh.primitives[j].extras);
            addHash(pSubmesh, "signature", hashes[j]);
            cJSON_AddItemToArray(pSubmeshes, pSubmesh);
        }
        qsort(hashes.data(), hashes.size(), sizeof(uint64_t), compareHashes);
        addHash(meshes[i], "signature", hashBytes(hashes.data(), hashes.size() * sizeof(uint64_t)));
        cJSON_AddItemToArray(pRecords, meshes[i]);
    }
    if (!matchIdentities(pOldRecords, pRecords))
        return false;

    pAsset = cJSON_CreateObject();
    cJSON_AddStringToObject(pAsset, "format", "Horizon.SceneAsset");
    cJSON_AddNumberToObject(pAsset, "version", 1);
    cJSON_AddStringToObject(pAsset, "id", text(pMetadata, "scene"));
    cJSON_AddStringToObject(pAsset, "source", pSourceFile);
    cJSON_AddStringToObject(pAsset, "contentHash", text(&legacyManifest, "contentHash"));
    cJSON* pTextures = cJSON_AddArrayToObject(pAsset, "textures");
    for (uint32_t i = 0; i < textures.size(); ++i)
    {
        cJSON* pTexture = cJSON_Duplicate(cJSON_GetArrayItem(pLegacyTextures, (int)i), true);
        cJSON_AddStringToObject(pTexture, "id", text(textures[i], "id"));
        cJSON_AddItemToArray(pTextures, pTexture);
    }
    cJSON* pMaterials = cJSON_AddArrayToObject(pAsset, "materials");
    for (uint32_t i = 0; i < materials.size(); ++i)
    {
        cJSON* pMaterial = cJSON_Duplicate(cJSON_GetArrayItem(pLegacyMaterials, (int)i), true);
        cJSON_AddStringToObject(pMaterial, "id", text(materials[i], "id"));
        const char* fields[] = { "baseColorTexture", "normalTexture", "metallicRoughnessTexture", "emissiveTexture" };
        for (const char* pField : fields)
        {
            const int index = cJSON_GetObjectItemCaseSensitive(pMaterial, pField)->valueint;
            cJSON_ReplaceItemInObjectCaseSensitive(
                pMaterial, pField, index >= 0 ? cJSON_CreateString(text(textures[(uint32_t)index], "id")) : cJSON_CreateNull());
        }
        cJSON_AddNumberToObject(pMaterial, "alphaMode", data.materials[i].alpha_mode);
        cJSON_AddNumberToObject(pMaterial, "alphaCutoff", data.materials[i].alpha_cutoff);
        cJSON_AddBoolToObject(pMaterial, "doubleSided", data.materials[i].double_sided);
        cJSON_AddItemToArray(pMaterials, pMaterial);
    }
    cJSON*   pMeshes = cJSON_AddArrayToObject(pAsset, "meshes");
    uint32_t drawIndex = 0;
    for (uint32_t i = 0; i < meshes.size(); ++i)
    {
        cJSON* pMesh = cJSON_CreateObject();
        cJSON_AddStringToObject(pMesh, "id", text(meshes[i], "id"));
        cJSON_AddStringToObject(pMesh, "name", text(meshes[i], "name"));
        cJSON_AddStringToObject(pMesh, "geometry", pGeometryFile);
        cJSON* pSubmeshes = cJSON_AddArrayToObject(pMesh, "submeshes");
        for (cgltf_size j = 0; j < data.meshes[i].primitives_count; ++j)
        {
            cJSON* pSubmesh = cJSON_CreateObject();
            cJSON_AddStringToObject(pSubmesh, "id",
                                    text(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(meshes[i], "submeshes"), (int)j), "id"));
            cJSON_AddNumberToObject(pSubmesh, "draw", drawIndex++);
            const cgltf_material* pMaterial = data.meshes[i].primitives[j].material;
            cJSON_AddItemToObject(pSubmesh, "material",
                                  pMaterial ? cJSON_CreateString(text(materials[(uint32_t)(pMaterial - data.materials)], "id"))
                                            : cJSON_CreateNull());
            cJSON_AddItemToArray(pSubmeshes, pSubmesh);
        }
        cJSON_AddItemToArray(pMeshes, pMesh);
    }
    cJSON*              pNodes = cJSON_AddArrayToObject(pAsset, "nodes");
    hz::Array<uint32_t> selected((uint32_t)data.nodes_count);
    hz::Array<uint32_t> stack;
    for (uint32_t& index : selected)
        index = UINT32_MAX;
    const cgltf_scene* pScene = data.scene ? data.scene : (data.scenes_count ? &data.scenes[0] : nullptr);
    if (pScene)
    {
        for (cgltf_size i = pScene->nodes_count; i > 0; --i)
            stack.pushBack((uint32_t)(pScene->nodes[i - 1] - data.nodes));
    }
    else
        for (uint32_t i = selected.size(); i > 0; --i)
            if (!data.nodes[i - 1].parent)
                stack.pushBack(i - 1);
    uint32_t nodeCount = 0;
    while (!stack.empty())
    {
        const uint32_t index = stack[stack.size() - 1];
        stack.popBack();
        if (selected[index] != UINT32_MAX)
        {
            LOGF(eERROR, "Scene node occurs more than once in the selected hierarchy");
            return false;
        }
        const cgltf_node& node = data.nodes[index];
        selected[index] = nodeCount++;
        cJSON* pNode = cJSON_CreateObject();
        cJSON_AddStringToObject(pNode, "name", node.name ? node.name : "");
        cJSON_AddNumberToObject(pNode, "parent", node.parent ? (double)selected[node.parent - data.nodes] : -1.0);
        if (node.has_matrix)
            addFloats(pNode, "matrix", node.matrix, 16);
        else
        {
            addFloats(pNode, "translation", node.translation, 3);
            addFloats(pNode, "rotation", node.rotation, 4);
            addFloats(pNode, "scale", node.scale, 3);
        }
        if (node.mesh)
            cJSON_AddStringToObject(pNode, "mesh", text(meshes[(uint32_t)(node.mesh - data.meshes)], "id"));
        cJSON_AddItemToArray(pNodes, pNode);
        for (cgltf_size i = node.children_count; i > 0; --i)
            stack.pushBack((uint32_t)(node.children[i - 1] - data.nodes));
    }
    return true;
}

bool SceneAssetCooker::write() const
{
    ASSERT(pMetadata && pAsset);
    uint64_t hash = 0;
    if (!writeJson(sourceDirectory, metadataFile, pMetadata, &hash))
        return false;
    addHash(pAsset, "identityHash", hash);
    return writeJson(outputDirectory, assetFile, pAsset);
}
