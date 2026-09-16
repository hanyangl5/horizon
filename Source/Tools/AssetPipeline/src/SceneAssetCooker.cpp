/* Copyright (c) 2026 Horizon */

#include "SceneAssetCooker.h"
#include "Core/IContainer.h"
#include "Core/IToolFileSystem.h"
#include "Scene/SceneID.h"
#include "Scene/SceneAsset.h"
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

static bool recoverJson(ResourceDirectory directory, const char* pPath)
{
    char backup[FS_MAX_PATH];
    if (snprintf(backup, sizeof(backup), "%s.bak", pPath) >= (int)sizeof(backup))
        return false;
    if (!fsFileExist(directory, backup))
        return true;
    if (!fsFileExist(directory, pPath))
        return fsRenameFile(directory, backup, pPath);
    return true;
}

static bool validTextureIdentity(const cJSON* pDocument)
{
    const cJSON* pVersion = cJSON_GetObjectItemCaseSensitive(pDocument, "version");
    hz::AssetID  linear, srgb;
    const cJSON* pLinear = cJSON_GetObjectItemCaseSensitive(pDocument, "linear");
    const cJSON* pSrgb = cJSON_GetObjectItemCaseSensitive(pDocument, "srgb");
    return strcmp(text(pDocument, "format"), "Horizon.TextureIdentity") == 0 && cJSON_IsNumber(pVersion) && pVersion->valuedouble == 1 &&
           (pLinear || pSrgb) && (!pLinear || (cJSON_IsString(pLinear) && linear.parse(pLinear->valuestring) && linear.isValid())) &&
           (!pSrgb || (cJSON_IsString(pSrgb) && srgb.parse(pSrgb->valuestring) && srgb.isValid())) && (!linear.isValid() || linear != srgb);
}

static bool writeJson(ResourceDirectory directory, const char* pPath, const cJSON* pValue)
{
    if (!recoverJson(directory, pPath))
        return false;
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
    if ((fsFileExist(directory, backup) && !fsRemoveFile(directory, backup)) || (exists && !fsRenameFile(directory, pPath, backup)))
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
        if (*text(pRecord, "registeredID"))
            continue;
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
        if (*text(pRecord, "registeredID"))
            snprintf(id, sizeof(id), "%s", text(pRecord, "registeredID"));
        else if (pMatch)
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
    cJSON_Delete(pTextureMetadata);
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
    const bool   current = metadata.get() && cJSON_IsNumber(pVersion) && pVersion->valuedouble == 1 &&
                         strcmp(text(asset.get(), "format"), "Horizon.SceneAsset") == 0 &&
                         strcmp(text(asset.get(), "contentHash"), pContentHash) == 0 &&
                         strcmp(text(asset.get(), "identityHash"), value) == 0;
    if (!current)
        return false;
    const cJSON* pTextures = cJSON_GetObjectItemCaseSensitive(asset.get(), "textures");
    for (const cJSON* pTexture = pTextures ? pTextures->child : nullptr; pTexture; pTexture = pTexture->next)
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(pTexture, "cooked")) && !fsFileExist(outputDirectory, text(pTexture, "path")))
            return false;
    const cJSON* pRecords = cJSON_GetObjectItemCaseSensitive(metadata.get(), "assets");
    for (const cJSON* pRecord = pRecords ? pRecords->child : nullptr; pRecord; pRecord = pRecord->next)
    {
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(pRecord, "missing")) || !*text(pRecord, "source"))
            continue;
        char path[FS_MAX_PATH];
        if (snprintf(path, sizeof(path), "%s.asset.json", text(pRecord, "source")) >= (int)sizeof(path))
            return false;
        const JsonDocument registered(readJson(sourceDirectory, path));
        const char*        pVariant = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(pRecord, "srgb")) ? "srgb" : "linear";
        if (!validTextureIdentity(registered.get()) || strcmp(text(registered.get(), pVariant), text(pRecord, "id")) != 0)
            return false;
    }
    return true;
}

bool SceneAssetCooker::prepare(const cgltf_data& data)
{
    ASSERT(!pMetadata && !pAsset);
    if (!recoverJson(sourceDirectory, metadataFile))
        return false;
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
    return prepareTextures(data);
}

bool SceneAssetCooker::registerTexture(CookedSceneTexture& texture)
{
    if (!texture.source[0])
        return true;
    char path[FS_MAX_PATH];
    if (snprintf(path, sizeof(path), "%s.asset.json", texture.source) >= (int)sizeof(path))
        return false;
    cJSON* pEntry = nullptr;
    for (cJSON* pItem = pTextureMetadata->child; pItem; pItem = pItem->next)
        if (strcmp(text(pItem, "path"), path) == 0)
            pEntry = pItem;
    if (!pEntry)
    {
        if (!recoverJson(sourceDirectory, path))
            return false;
        const bool exists = fsFileExist(sourceDirectory, path);
        cJSON*     pDocument = exists ? readJson(sourceDirectory, path) : cJSON_CreateObject();
        pEntry = cJSON_CreateObject();
        cJSON_AddStringToObject(pEntry, "path", path);
        cJSON_AddItemToObject(pEntry, "document", pDocument);
        cJSON_AddItemToArray(pTextureMetadata, pEntry);
        if (exists)
        {
            if (!validTextureIdentity(pDocument))
            {
                LOGF(eERROR, "Invalid texture identity sidecar '%s'; refusing to replace persistent IDs", path);
                return false;
            }
        }
        else
        {
            cJSON_AddStringToObject(pDocument, "format", "Horizon.TextureIdentity");
            cJSON_AddNumberToObject(pDocument, "version", 1);
        }
    }
    cJSON*       pDocument = cJSON_GetObjectItemCaseSensitive(pEntry, "document");
    const char*  pVariant = texture.srgb ? "srgb" : "linear";
    const char*  pID = text(pDocument, pVariant);
    const cJSON* pOldRecords = cJSON_GetObjectItemCaseSensitive(pPrevious, "assets");
    for (const cJSON* pRecord = pOldRecords ? pOldRecords->child : nullptr; pRecord; pRecord = pRecord->next)
        if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(pRecord, "missing")) && strcmp(text(pRecord, "source"), texture.source) == 0 &&
            (bool)cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(pRecord, "srgb")) == texture.srgb && strcmp(text(pRecord, "id"), pID) != 0)
        {
            LOGF(eERROR, "Missing or changed texture identity in '%s'; restore the sidecar before recooking", path);
            return false;
        }
    if (*pID)
        return texture.registeredID.parse(pID);
    texture.registeredID = hz::AssetID::create();
    if (!texture.registeredID.isValid())
        return false;
    char id[hz::kIDStringCapacity];
    texture.registeredID.toString(id);
    cJSON_AddStringToObject(pDocument, pVariant, id);
    if (!cJSON_GetObjectItemCaseSensitive(pEntry, "dirty"))
        cJSON_AddBoolToObject(pEntry, "dirty", true);
    return true;
}

bool SceneAssetCooker::prepareTextures(const cgltf_data& data)
{
    pTextureMetadata = cJSON_CreateArray();
    textures.resize((uint32_t)data.textures_count);
    const SceneTextureCooker cooker(sourceDirectory, outputDirectory, pSourceFile);
    for (uint32_t i = 0; i < textures.size(); ++i)
    {
        const cgltf_texture& texture = data.textures[i];
        bool                 srgb = false;
        bool                 linear = false;
        for (cgltf_size j = 0; j < data.materials_count; ++j)
        {
            if (data.materials[j].pbr_metallic_roughness.base_color_texture.texture == &texture ||
                data.materials[j].emissive_texture.texture == &texture)
                srgb = true;
            if (data.materials[j].pbr_metallic_roughness.metallic_roughness_texture.texture == &texture ||
                data.materials[j].normal_texture.texture == &texture || data.materials[j].occlusion_texture.texture == &texture)
                linear = true;
        }
        if (srgb && linear)
        {
            LOGF(eERROR, "Texture %u in '%s' is used as both color and linear data; use separate texture entries for the same image", i,
                 pSourceFile);
            return false;
        }
        bool reused = false;
        for (uint32_t j = 0; j < i; ++j)
            if (data.textures[j].image == texture.image && textures[j].srgb == srgb)
            {
                textures[i] = textures[j];
                reused = true;
                break;
            }
        if (reused)
            continue;
        if (!texture.image || !cooker.cook(*texture.image, srgb, textures[i]) || !registerTexture(textures[i]))
        {
            LOGF(eERROR, "Cannot cook texture %u from '%s'", i, pSourceFile);
            return false;
        }
        for (uint32_t j = 0; j < i; ++j)
            if (textures[i].registeredID.isValid() && textures[i].registeredID == textures[j].registeredID &&
                (strcmp(textures[i].source, textures[j].source) != 0 || textures[i].srgb != textures[j].srgb))
            {
                LOGF(eERROR, "Texture sidecars assign the same ID to different resources in '%s'", pSourceFile);
                return false;
            }
    }
    return true;
}

cJSON* SceneAssetCooker::createMaterial(const cgltf_data& data, uint32_t index, hz::Span<cJSON*> textureRecords, bool signature) const
{
    const cgltf_material&               material = data.materials[index];
    const cgltf_pbr_metallic_roughness* pPbr = material.has_pbr_metallic_roughness ? &material.pbr_metallic_roughness : nullptr;
    cJSON*                              pMaterial = cJSON_CreateObject();
    if (!signature)
        cJSON_AddStringToObject(pMaterial, "name", material.name ? material.name : "material");
    const char*          fields[] = { "baseColorTexture", "normalTexture", "metallicRoughnessTexture", "emissiveTexture" };
    const cgltf_texture* sources[] = {
        pPbr ? pPbr->base_color_texture.texture : nullptr,
        material.normal_texture.texture,
        pPbr ? pPbr->metallic_roughness_texture.texture : nullptr,
        material.emissive_texture.texture,
    };
    for (uint32_t i = 0; i < 4; ++i)
    {
        const char* pReference = sources[i] ? text(textureRecords.pData[sources[i] - data.textures], signature ? "signature" : "id") : "";
        cJSON_AddItemToObject(pMaterial, fields[i], *pReference || signature ? cJSON_CreateString(pReference) : cJSON_CreateNull());
    }
    const float defaultBaseColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    addFloats(pMaterial, "baseColorFactor", pPbr ? pPbr->base_color_factor : defaultBaseColor, 4);
    cJSON_AddNumberToObject(pMaterial, "metallicFactor", pPbr ? pPbr->metallic_factor : 1.0f);
    cJSON_AddNumberToObject(pMaterial, "roughnessFactor", pPbr ? pPbr->roughness_factor : 1.0f);
    addFloats(pMaterial, "emissiveFactor", material.emissive_factor, 3);
    cJSON_AddNumberToObject(pMaterial, "alphaMode", material.alpha_mode);
    cJSON_AddNumberToObject(pMaterial, "alphaCutoff", material.alpha_cutoff);
    cJSON_AddBoolToObject(pMaterial, "doubleSided", material.double_sided);
    return pMaterial;
}

bool SceneAssetCooker::build(const cgltf_data& data, const char* pContentHash)
{
    ASSERT(pMetadata && !pAsset);
    cJSON*            pOldRecords = cJSON_GetObjectItemCaseSensitive(pPrevious, "assets");
    cJSON*            pRecords = cJSON_AddArrayToObject(pMetadata, "assets");
    hz::Array<cJSON*> textures((uint32_t)data.textures_count);
    hz::Array<cJSON*> materials((uint32_t)data.materials_count);
    hz::Array<cJSON*> meshes((uint32_t)data.meshes_count);
    for (uint32_t i = 0; i < textures.size(); ++i)
    {
        const cgltf_texture& texture = data.textures[i];
        const CookedSceneTexture& cooked = this->textures[i];
        for (uint32_t j = 0; j < i; ++j)
            if ((data.textures[j].image == texture.image && this->textures[j].srgb == cooked.srgb) ||
                (cooked.registeredID.isValid() && this->textures[j].registeredID == cooked.registeredID))
            {
                textures[i] = textures[j];
                break;
            }
        if (textures[i])
            continue;
        textures[i] = identity("Texture", texture.name ? texture.name : (texture.image->name ? texture.image->name : cooked.source), data,
                               texture.extras);
        if (!*text(textures[i], "sourceKey"))
            addSourceKey(textures[i], data, texture.image->extras);
        addHash(textures[i], "signature", hashBytes(&cooked.srgb, sizeof(cooked.srgb), cooked.signature));
        cJSON_AddBoolToObject(textures[i], "srgb", cooked.srgb);
        if (cooked.source[0])
            cJSON_AddStringToObject(textures[i], "source", cooked.source);
        if (cooked.registeredID.isValid())
        {
            char id[hz::kIDStringCapacity];
            cooked.registeredID.toString(id);
            cJSON_AddStringToObject(textures[i], "registeredID", id);
        }
        cJSON_AddItemToArray(pRecords, textures[i]);
    }
    for (uint32_t i = 0; i < materials.size(); ++i)
    {
        const cgltf_material& material = data.materials[i];
        materials[i] = identity("Material", material.name, data, material.extras);
        // Texture array indices are not identity evidence.
        const JsonDocument signature(createMaterial(data, i, { textures.data(), textures.size() }, true));
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
    if (!matchIdentities(pOldRecords, pRecords) || !validateIdentities(pRecords) || containsID(pRecords, text(pMetadata, "scene")))
    {
        LOGF(eERROR, "Conflicting scene/subasset identities in '%s'", pSourceFile);
        return false;
    }

    pAsset = cJSON_CreateObject();
    cJSON_AddStringToObject(pAsset, "format", "Horizon.SceneAsset");
    cJSON_AddNumberToObject(pAsset, "version", 1);
    cJSON_AddStringToObject(pAsset, "id", text(pMetadata, "scene"));
    cJSON_AddStringToObject(pAsset, "source", pSourceFile);
    cJSON_AddStringToObject(pAsset, "contentHash", pContentHash);
    cJSON* pTextures = cJSON_AddArrayToObject(pAsset, "textures");
    for (uint32_t i = 0; i < textures.size(); ++i)
    {
        if (containsID(pTextures, text(textures[i], "id")))
            continue;
        cJSON* pTexture = cJSON_CreateObject();
        cJSON_AddStringToObject(pTexture, "path", this->textures[i].path);
        cJSON_AddBoolToObject(pTexture, "srgb", this->textures[i].srgb);
        cJSON_AddBoolToObject(pTexture, "cooked", this->textures[i].cooked);
        cJSON_AddStringToObject(pTexture, "id", text(textures[i], "id"));
        cJSON_AddItemToArray(pTextures, pTexture);
    }
    cJSON* pMaterials = cJSON_AddArrayToObject(pAsset, "materials");
    for (uint32_t i = 0; i < materials.size(); ++i)
    {
        cJSON* pMaterial = createMaterial(data, i, { textures.data(), textures.size() }, false);
        cJSON_AddStringToObject(pMaterial, "id", text(materials[i], "id"));
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
    return validate();
}

bool SceneAssetCooker::validate() const
{
    char* pMetadataJson = cJSON_Print(pMetadata);
    if (!pMetadataJson)
        return false;
    addHash(pAsset, "identityHash", hashBytes(pMetadataJson, strlen(pMetadataJson)));
    cJSON_free(pMetadataJson);
    char* pJson = cJSON_PrintUnformatted(pAsset);
    if (!pJson)
        return false;
    hz::SceneAsset asset;
    const bool     valid = asset.parse({ pJson, (uint32_t)strlen(pJson) });
    cJSON_free(pJson);
    return valid;
}

bool SceneAssetCooker::write() const
{
    ASSERT(pMetadata && pAsset);
    for (const cJSON* pEntry = pTextureMetadata->child; pEntry; pEntry = pEntry->next)
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(pEntry, "dirty")) &&
            !writeJson(sourceDirectory, text(pEntry, "path"), cJSON_GetObjectItemCaseSensitive(pEntry, "document")))
            return false;
    if (!writeJson(sourceDirectory, metadataFile, pMetadata))
        return false;
    return writeJson(outputDirectory, assetFile, pAsset);
}
