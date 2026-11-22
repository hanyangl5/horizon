/* Copyright (c) 2026 Horizon */

#include "SceneAssetCooker.h"
#include "CookUtilities.h"
#include "Core/IContainer.h"
#include "Core/IToolFileSystem.h"
#include "Core/IMath.h"
#include "Scene/SceneID.h"
#include "Scene/SceneAsset.h"
#include "Scene/AssetCatalog.h"
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
    cJSON*        Get() const { return pValue; }

private:
    cJSON* pValue;
};

#include "Core/IMemory.h"

static const char* text(const cJSON* pObject, const char* pField)
{
    const cJSON* pValue = cJSON_GetObjectItemCaseSensitive(pObject, pField);
    return cJSON_IsString(pValue) ? pValue->valuestring : "";
}

static void addHash(cJSON* pObject, const char* pField, uint64_t Hash)
{
    char value[17];
    snprintf(value, sizeof(value), "%016llx", (unsigned long long)Hash);
    cJSON_AddStringToObject(pObject, pField, value);
}

static cJSON* readJson(ResourceDirectory directory, const char* pPath, uint64_t* pHash = nullptr)
{
    FileStream stream = {};
    if (!OpenStreamFromPath(directory, pPath, FileModeRead, &stream))
    {
        return nullptr;
    }
    const ssize_t Size = GetStreamFileSize(&stream);
    if (Size <= 0 || Size > 64 * 1024 * 1024)
    {
        CloseStream(&stream);
        return nullptr;
    }
    hz::Array<char> bytes((uint32_t)Size + 1);
    const bool      read = ReadFromStream(&stream, bytes.GetData(), (size_t)Size) == (size_t)Size;
    CloseStream(&stream);
    if (!read)
        return nullptr;
    if (pHash)
        *pHash = cookHash(bytes.GetData(), (size_t)Size);
    return cJSON_ParseWithLength(bytes.GetData(), bytes.Size());
}

static bool recoverJson(ResourceDirectory directory, const char* pPath)
{
    char backup[FS_MAX_PATH];
    if (snprintf(backup, sizeof(backup), "%s.bak", pPath) >= (int)sizeof(backup))
        return false;
    if (!FileExists(directory, backup))
    {
        return true;
    }
    if (!FileExists(directory, pPath))
    {
        return RenameFile(directory, backup, pPath);
    }
    return true;
}

static bool validTextureIdentity(const cJSON* pDocument)
{
    const cJSON* pVersion = cJSON_GetObjectItemCaseSensitive(pDocument, "version");
    hz::AssetID  linear, srgb;
    const cJSON* pLinear = cJSON_GetObjectItemCaseSensitive(pDocument, "linear");
    const cJSON* pSrgb = cJSON_GetObjectItemCaseSensitive(pDocument, "srgb");
    return strcmp(text(pDocument, "format"), "Horizon.TextureIdentity") == 0 && cJSON_IsNumber(pVersion) && pVersion->valuedouble == 1 &&
           (pLinear || pSrgb) && (!pLinear || (cJSON_IsString(pLinear) && linear.Parse(pLinear->valuestring) && linear.IsValid())) &&
           (!pSrgb || (cJSON_IsString(pSrgb) && srgb.Parse(pSrgb->valuestring) && srgb.IsValid())) && (!linear.IsValid() || linear != srgb);
}

static bool writeBytes(ResourceDirectory directory, const char* pPath, const char* pData, uint32_t size)
{
    CookedFile file(directory, pPath);
    file.Write(pData, size);
    if (file.Publish())
    {
        return true;
    }
    LOGF(Error, "Cannot publish scene asset file '%s'", pPath);
    return false;
}

static bool writeJson(ResourceDirectory directory, const char* pPath, const cJSON* pValue)
{
    char* pText = cJSON_Print(pValue);
    if (!pText)
        return false;
    const bool written = writeBytes(directory, pPath, pText, (uint32_t)strlen(pText));
    cJSON_free(pText);
    return written;
}

static bool buildCatalog(const cJSON* pAsset, const char* pAssetFile, hz::Array<char>& output)
{
    char* pJson = cJSON_PrintUnformatted(pAsset);
    if (!pJson)
        return false;
    hz::SceneAsset asset;
    const bool     valid = asset.Parse({ pJson, (uint32_t)strlen(pJson) });
    cJSON_free(pJson);
    hz::AssetCatalog catalog;
    return valid && catalog.AddScene(asset, pAssetFile) && catalog.Serialize(output);
}

static void addFloats(cJSON* pObject, const char* pField, const float* pValues, uint32_t Count)
{
    cJSON* pArray = cJSON_AddArrayToObject(pObject, pField);
    for (uint32_t i = 0; i < Count; ++i)
    {
        cJSON_AddItemToArray(pArray, cJSON_CreateNumber(pValues[i]));
    }
}

static void addSourceKey(cJSON* pObject, const cgltf_data& data, const cgltf_extras& extras)
{
    if (extras.end_offset <= extras.start_offset)
        return;
    const JsonDocument json(cJSON_ParseWithLength(data.json + extras.start_offset, extras.end_offset - extras.start_offset));
    const char*        pKey = text(json.Get(), "horizonId");
    if (*pKey)
        cJSON_AddStringToObject(pObject, "sourceKey", pKey);
}

static cJSON* identity(const char* pKind, const char* Name, const cgltf_data& data, const cgltf_extras& extras)
{
    cJSON* pRecord = cJSON_CreateObject();
    cJSON_AddStringToObject(pRecord, "kind", pKind);
    cJSON_AddStringToObject(pRecord, "name", Name ? Name : "");
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
    uint32_t Count = 0;
    cJSON*   pCandidate = nullptr;
    for (cJSON* pItem = pOld ? pOld->child : nullptr; pItem; pItem = pItem->next)
        if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(pItem, "missing")) && sameField(pRecord, pItem, pField))
        {
            pCandidate = pItem;
            ++Count;
        }
    if (Count != 1)
    {
        return nullptr;
    }
    Count = 0;
    for (cJSON* pItem = pNew->child; pItem; pItem = pItem->next)
        if (sameField(pRecord, pItem, pField))
            ++Count;
    return Count == 1 ? pCandidate : nullptr;
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
            cJSON* Name = uniqueMatch(pOld, pNew, pRecord, "name");
            cJSON* pSignature = uniqueMatch(pOld, pNew, pRecord, "signature");
            matches[index] = Name && pSignature && Name != pSignature ? nullptr : (pSignature ? pSignature : Name);
            if (matches[index] && *text(matches[index], "sourceKey"))
                matches[index] = nullptr;
        }
    }
    index = 0;
    for (cJSON* pRecord = pNew->child; pRecord; pRecord = pRecord->next, ++index)
    {
        cJSON* pMatch = matches[index];
        for (uint32_t i = 0; pMatch && i < matches.Size(); ++i)
        {
            if (i != index && matches[i] == pMatch)
                pMatch = nullptr;
        }
        char id[hz::kIDStringCapacity];
        if (*text(pRecord, "registeredID"))
            snprintf(id, sizeof(id), "%s", text(pRecord, "registeredID"));
        else if (pMatch)
            snprintf(id, sizeof(id), "%s", text(pMatch, "id"));
        else
        {
            const hz::AssetID generated = hz::AssetID::Create();
            if (!generated.IsValid())
            {
                return false;
            }
            if (submeshes)
                snprintf(id, sizeof(id), "%016llx", (unsigned long long)generated.low);
            else
                generated.ToString(id);
            if (containsID(pOld, id) || containsID(pNew, id))
            {
                LOGF(Error, "Generated scene asset identity collision");
                return false;
            }
            if (pOld && pOld->child)
                LOGF(Warning, "No unique identity match for %s '%s'; assigning a new ID", text(pRecord, "kind"), text(pRecord, "name"));
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
        else if (!id.Parse(pID) || !id.IsValid())
        {
            return false;
        }
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
    uint64_t Hash = cookHash(&pAccessor->type, sizeof(pAccessor->type));
    Hash = cookHash(&pAccessor->count, sizeof(pAccessor->count), Hash);
    if (pAccessor->is_sparse)
    {
        hz::Array<float> values((uint32_t)(pAccessor->count * cgltf_num_components(pAccessor->type)));
        if (cgltf_accessor_unpack_floats(pAccessor, values.GetData(), values.Size()) != values.Size())
        {
            return 0;
        }
        return cookHash(values.GetData(), values.Size() * sizeof(float), Hash);
    }
    for (cgltf_size i = 0; i < pAccessor->count; ++i)
    {
        float values[16] = {};
        if (!cgltf_accessor_read_float(pAccessor, i, values, 16))
            return 0;
        Hash = cookHash(values, cgltf_num_components(pAccessor->type) * sizeof(float), Hash);
    }
    return Hash;
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
    for (uint32_t i = 0; i < attributes.Size(); ++i)
    {
        const cgltf_attribute& attribute = primitive.attributes[i];
        attributes[i] = accessorHash(attribute.data);
        attributes[i] = cookHash(attribute.name, strlen(attribute.name), attributes[i]);
    }
    qsort(attributes.GetData(), attributes.Size(), sizeof(uint64_t), compareHashes);
    uint64_t Hash = cookHash(attributes.GetData(), attributes.Size() * sizeof(uint64_t));
    Hash = cookHash(&primitive.type, sizeof(primitive.type), Hash);
    if (primitive.indices)
        for (cgltf_size i = 0; i < primitive.indices->count; ++i)
        {
            const uint64_t value = cgltf_accessor_read_index(primitive.indices, i);
            Hash = cookHash(&value, sizeof(value), Hash);
        }
    return Hash;
}

SceneAssetCooker::SceneAssetCooker(ResourceDirectory sourceDirectory, const char* pSourceFile, ResourceDirectory outputDirectory,
                                   const char* pGeometryFile):
    sourceDirectory(sourceDirectory),
    outputDirectory(outputDirectory), pSourceFile(pSourceFile), pGeometryFile(pGeometryFile)
{
    AppendPathExtension(pSourceFile, "asset.json", metadataFile);
    ReplacePathExtension(pGeometryFile, "sceneasset.json", assetFile);
    ReplacePathExtension(pGeometryFile, "assetcatalog.json", catalogFile);
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
    if (!FileExists(sourceDirectory, metadataFile) || !FileExists(outputDirectory, assetFile) || !FileExists(outputDirectory, catalogFile))
    {
        return false;
    }
    uint64_t           Hash = 0;
    const JsonDocument metadata(readJson(sourceDirectory, metadataFile, &Hash));
    const JsonDocument asset(readJson(outputDirectory, assetFile));
    char               value[17];
    snprintf(value, sizeof(value), "%016llx", (unsigned long long)Hash);
    const cJSON* pVersion = cJSON_GetObjectItemCaseSensitive(asset.Get(), "version");
    const bool   current = metadata.Get() && cJSON_IsNumber(pVersion) && pVersion->valuedouble == 1 &&
                         strcmp(text(asset.Get(), "format"), "Horizon.SceneAsset") == 0 &&
                         strcmp(text(asset.Get(), "contentHash"), pContentHash) == 0 &&
                         strcmp(text(asset.Get(), "identityHash"), value) == 0;
    if (!current)
        return false;
    hz::AssetCatalog catalog;
    hz::Array<char>  expected, actual;
    if (!buildCatalog(asset.Get(), assetFile, expected) || !catalog.Load(outputDirectory, catalogFile) || !catalog.Serialize(actual) ||
        expected.Size() != actual.Size() || memcmp(expected.GetData(), actual.GetData(), actual.Size()))
    {
        return false;
    }
    const cJSON* pTextures = cJSON_GetObjectItemCaseSensitive(asset.Get(), "textures");
    for (const cJSON* pTexture = pTextures ? pTextures->child : nullptr; pTexture; pTexture = pTexture->next)
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(pTexture, "cooked")) && !FileExists(outputDirectory, text(pTexture, "path")))
        {
            return false;
        }
    const char* animationTables[] = { "skeletons", "animations" };
    for (const char* pTable : animationTables)
    {
        const cJSON* pFiles = cJSON_GetObjectItemCaseSensitive(asset.Get(), pTable);
        for (const cJSON* pFile = pFiles ? pFiles->child : nullptr; pFile; pFile = pFile->next)
        {
            if (!FileExists(outputDirectory, text(pFile, "path")))
            {
                return false;
            }
        }
    }
    const cJSON* pRecords = cJSON_GetObjectItemCaseSensitive(metadata.Get(), "assets");
    for (const cJSON* pRecord = pRecords ? pRecords->child : nullptr; pRecord; pRecord = pRecord->next)
    {
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(pRecord, "missing")) || !*text(pRecord, "source"))
            continue;
        char path[FS_MAX_PATH];
        if (snprintf(path, sizeof(path), "%s.asset.json", text(pRecord, "source")) >= (int)sizeof(path))
            return false;
        const JsonDocument registered(readJson(sourceDirectory, path));
        const char*        pVariant = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(pRecord, "srgb")) ? "srgb" : "linear";
        if (!validTextureIdentity(registered.Get()) || strcmp(text(registered.Get(), pVariant), text(pRecord, "id")) != 0)
        {
            return false;
        }
    }
    return true;
}

bool SceneAssetCooker::prepare(const cgltf_data& data)
{
    ASSERT(!pMetadata && !pAsset);
    if (!recoverJson(sourceDirectory, metadataFile))
        return false;
    const bool exists = FileExists(sourceDirectory, metadataFile);
    pPrevious = exists ? readJson(sourceDirectory, metadataFile) : nullptr;
    const cJSON* pVersion = cJSON_GetObjectItemCaseSensitive(pPrevious, "version");
    cJSON*       pOldRecords = cJSON_GetObjectItemCaseSensitive(pPrevious, "assets");
    hz::AssetID  sceneID;
    if (exists)
    {
        if (!cJSON_IsNumber(pVersion) || pVersion->valuedouble != 1 || !sceneID.Parse(text(pPrevious, "scene")) || !sceneID.IsValid() ||
            !validateIdentities(pOldRecords) || containsID(pOldRecords, text(pPrevious, "scene")))
        {
            LOGF(Error, "Invalid scene identity sidecar '%s'; refusing to Replace persistent IDs", metadataFile);
            return false;
        }
    }
    else
    {
        if (FileExists(outputDirectory, assetFile))
        {
            LOGF(Error, "Missing identity sidecar '%s'; restore it before recooking", metadataFile);
            return false;
        }
        sceneID = hz::AssetID::Create();
        if (!sceneID.IsValid())
        {
            return false;
        }
    }
    pMetadata = cJSON_CreateObject();
    cJSON_AddNumberToObject(pMetadata, "version", 1);
    char id[hz::kIDStringCapacity];
    sceneID.ToString(id);
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
        const bool exists = FileExists(sourceDirectory, path);
        cJSON*     pDocument = exists ? readJson(sourceDirectory, path) : cJSON_CreateObject();
        pEntry = cJSON_CreateObject();
        cJSON_AddStringToObject(pEntry, "path", path);
        cJSON_AddItemToObject(pEntry, "document", pDocument);
        cJSON_AddItemToArray(pTextureMetadata, pEntry);
        if (exists)
        {
            if (!validTextureIdentity(pDocument))
            {
                LOGF(Error, "Invalid texture identity sidecar '%s'; refusing to Replace persistent IDs", path);
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
            LOGF(Error, "Missing or changed texture identity in '%s'; restore the sidecar before recooking", path);
            return false;
        }
    if (*pID)
        return texture.registeredID.Parse(pID);
    texture.registeredID = hz::AssetID::Create();
    if (!texture.registeredID.IsValid())
    {
        return false;
    }
    char id[hz::kIDStringCapacity];
    texture.registeredID.ToString(id);
    cJSON_AddStringToObject(pDocument, pVariant, id);
    if (!cJSON_GetObjectItemCaseSensitive(pEntry, "dirty"))
        cJSON_AddBoolToObject(pEntry, "dirty", true);
    return true;
}

bool SceneAssetCooker::prepareTextures(const cgltf_data& data)
{
    pTextureMetadata = cJSON_CreateArray();
    textures.Resize((uint32_t)data.textures_count);
    const SceneTextureCooker cooker(sourceDirectory, outputDirectory, pSourceFile);
    for (uint32_t i = 0; i < textures.Size(); ++i)
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
            LOGF(Error, "Texture %u in '%s' is used as both color and linear data; use separate texture entries for the same image", i,
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
            LOGF(Error, "Cannot cook texture %u from '%s'", i, pSourceFile);
            return false;
        }
        LOGF(Info, "Texture %u/%u %s", i + 1, textures.Size(), textures[i].source[0] ? textures[i].source : "(embedded)");
        for (uint32_t j = 0; j < i; ++j)
            if (textures[i].registeredID.IsValid() && textures[i].registeredID == textures[j].registeredID &&
                (strcmp(textures[i].source, textures[j].source) != 0 || textures[i].srgb != textures[j].srgb))
            {
                LOGF(Error, "Texture sidecars assign the same ID to different resources in '%s'", pSourceFile);
                return false;
            }
    }
    return true;
}

cJSON* SceneAssetCooker::createMaterial(const cgltf_data& data, const cgltf_material& material, hz::Span<cJSON*> textureRecords,
                                        bool signature) const
{
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
        const char* pReference = sources[i] ? text(textureRecords.Data[sources[i] - data.textures], signature ? "signature" : "id") : "";
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
    cJSON* pOldRecords = cJSON_GetObjectItemCaseSensitive(pPrevious, "assets");
    cJSON* pRecords = cJSON_AddArrayToObject(pMetadata, "assets");
    bool   needsDefaultMaterial = false;
    for (cgltf_size i = 0; i < data.meshes_count; ++i)
        for (cgltf_size j = 0; j < data.meshes[i].primitives_count; ++j)
            needsDefaultMaterial |= !data.meshes[i].primitives[j].material;
    cgltf_material defaultMaterial = {};
    defaultMaterial.name = (char*)"Default Material";
    defaultMaterial.alpha_cutoff = 0.5f;
    hz::Array<cJSON*> textures((uint32_t)data.textures_count);
    hz::Array<cJSON*> materials((uint32_t)data.materials_count + needsDefaultMaterial);
    hz::Array<cJSON*> meshes((uint32_t)data.meshes_count);
    for (uint32_t i = 0; i < textures.Size(); ++i)
    {
        const cgltf_texture&      texture = data.textures[i];
        const CookedSceneTexture& cooked = this->textures[i];
        for (uint32_t j = 0; j < i; ++j)
            if ((data.textures[j].image == texture.image && this->textures[j].srgb == cooked.srgb) ||
                (cooked.registeredID.IsValid() && this->textures[j].registeredID == cooked.registeredID))
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
        addHash(textures[i], "signature", cookHash(&cooked.srgb, sizeof(cooked.srgb), cooked.signature));
        cJSON_AddBoolToObject(textures[i], "srgb", cooked.srgb);
        if (cooked.source[0])
            cJSON_AddStringToObject(textures[i], "source", cooked.source);
        if (cooked.registeredID.IsValid())
        {
            char id[hz::kIDStringCapacity];
            cooked.registeredID.ToString(id);
            cJSON_AddStringToObject(textures[i], "registeredID", id);
        }
        cJSON_AddItemToArray(pRecords, textures[i]);
    }
    for (uint32_t i = 0; i < materials.Size(); ++i)
    {
        const cgltf_material& material = i < data.materials_count ? data.materials[i] : defaultMaterial;
        materials[i] = identity(i < data.materials_count ? "material" : "DefaultMaterial", material.name, data, material.extras);
        // Texture array indices are not identity evidence.
        const JsonDocument signature(createMaterial(data, material, { textures.GetData(), textures.Size() }, true));
        char*              pText = cJSON_PrintUnformatted(signature.Get());
        addHash(materials[i], "signature", cookHash(pText, strlen(pText)));
        cJSON_free(pText);
        cJSON_AddItemToArray(pRecords, materials[i]);
    }
    for (uint32_t i = 0; i < meshes.Size(); ++i)
    {
        const cgltf_mesh& mesh = data.meshes[i];
        meshes[i] = identity("mesh", mesh.name, data, mesh.extras);
        cJSON*              pSubmeshes = cJSON_AddArrayToObject(meshes[i], "submeshes");
        hz::Array<uint64_t> hashes((uint32_t)mesh.primitives_count);
        for (uint32_t j = 0; j < hashes.Size(); ++j)
        {
            hashes[j] = primitiveHash(mesh.primitives[j]);
            cJSON* pSubmesh = identity("Submesh", nullptr, data, mesh.primitives[j].extras);
            addHash(pSubmesh, "signature", hashes[j]);
            cJSON_AddItemToArray(pSubmeshes, pSubmesh);
        }
        qsort(hashes.GetData(), hashes.Size(), sizeof(uint64_t), compareHashes);
        addHash(meshes[i], "signature", cookHash(hashes.GetData(), hashes.Size() * sizeof(uint64_t)));
        cJSON_AddItemToArray(pRecords, meshes[i]);
    }
    char baseName[FS_MAX_PATH];
    snprintf(baseName, sizeof(baseName), "%s", pGeometryFile);
    char* pExtension = strrchr(baseName, '.');
    if (pExtension && !strchr(pExtension, '/'))
    {
        *pExtension = 0;
    }
    if (!skeletonCooker.cook(data, baseName))
    {
        LOGF(Error, "Cannot cook skins and animations of '%s'", pSourceFile);
        return false;
    }
    hz::Array<cJSON*> skeletons(skeletonCooker.skeletons.Size());
    hz::Array<cJSON*> animations(skeletonCooker.animations.Size());
    for (uint32_t i = 0; i < skeletons.Size(); ++i)
    {
        const CookedSkeleton& skeleton = skeletonCooker.skeletons[i];
        skeletons[i] = identity("skeleton", skeleton.name, data, data.nodes[skeleton.roots[0]].extras);
        addHash(skeletons[i], "signature", skeleton.signature);
        cJSON_AddItemToArray(pRecords, skeletons[i]);
    }
    for (uint32_t i = 0; i < animations.Size(); ++i)
    {
        const CookedAnimation& animation = skeletonCooker.animations[i];
        animations[i] = identity("animation", animation.name, data, data.animations[animation.animation].extras);
        addHash(animations[i], "signature", animation.signature);
        cJSON_AddItemToArray(pRecords, animations[i]);
    }
    if (!matchIdentities(pOldRecords, pRecords) || !validateIdentities(pRecords) || containsID(pRecords, text(pMetadata, "scene")))
    {
        LOGF(Error, "Conflicting scene/subasset identities in '%s'", pSourceFile);
        return false;
    }

    pAsset = cJSON_CreateObject();
    cJSON_AddStringToObject(pAsset, "format", "Horizon.SceneAsset");
    cJSON_AddNumberToObject(pAsset, "version", 1);
    cJSON_AddStringToObject(pAsset, "id", text(pMetadata, "scene"));
    cJSON_AddStringToObject(pAsset, "source", pSourceFile);
    cJSON_AddStringToObject(pAsset, "contentHash", pContentHash);
    cJSON* pTextures = cJSON_AddArrayToObject(pAsset, "textures");
    for (uint32_t i = 0; i < textures.Size(); ++i)
    {
        if (containsID(pTextures, text(textures[i], "id")))
            continue;
        cJSON* pTexture = cJSON_CreateObject();
        cJSON_AddStringToObject(pTexture, "path", this->textures[i].path);
        cJSON_AddStringToObject(pTexture, "source", this->textures[i].source[0] ? this->textures[i].source : pSourceFile);
        cJSON_AddBoolToObject(pTexture, "srgb", this->textures[i].srgb);
        cJSON_AddBoolToObject(pTexture, "cooked", this->textures[i].cooked);
        cJSON_AddStringToObject(pTexture, "id", text(textures[i], "id"));
        cJSON_AddItemToArray(pTextures, pTexture);
    }
    cJSON* pMaterials = cJSON_AddArrayToObject(pAsset, "materials");
    for (uint32_t i = 0; i < materials.Size(); ++i)
    {
        const cgltf_material& material = i < data.materials_count ? data.materials[i] : defaultMaterial;
        cJSON*                pMaterial = createMaterial(data, material, { textures.GetData(), textures.Size() }, false);
        cJSON_AddStringToObject(pMaterial, "id", text(materials[i], "id"));
        cJSON_AddItemToArray(pMaterials, pMaterial);
    }
    cJSON*   pMeshes = cJSON_AddArrayToObject(pAsset, "meshes");
    uint32_t drawIndex = 0;
    for (uint32_t i = 0; i < meshes.Size(); ++i)
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
            cJSON_AddStringToObject(
                pSubmesh, "material",
                text(materials[pMaterial ? (uint32_t)(pMaterial - data.materials) : (uint32_t)data.materials_count], "id"));
            cJSON_AddItemToArray(pSubmeshes, pSubmesh);
        }
        cJSON_AddItemToArray(pMeshes, pMesh);
    }
    if (!skeletons.Empty())
    {
        cJSON* pSkeletons = cJSON_AddArrayToObject(pAsset, "skeletons");
        for (uint32_t i = 0; i < skeletons.Size(); ++i)
        {
            cJSON* pSkeleton = cJSON_CreateObject();
            cJSON_AddStringToObject(pSkeleton, "id", text(skeletons[i], "id"));
            cJSON_AddStringToObject(pSkeleton, "name", skeletonCooker.skeletons[i].name);
            cJSON_AddStringToObject(pSkeleton, "path", skeletonCooker.skeletons[i].path);
            cJSON_AddItemToArray(pSkeletons, pSkeleton);
        }
    }
    if (!animations.Empty())
    {
        cJSON* pAnimations = cJSON_AddArrayToObject(pAsset, "animations");
        for (uint32_t i = 0; i < animations.Size(); ++i)
        {
            const CookedAnimation& animation = skeletonCooker.animations[i];
            cJSON*                 pAnimation = cJSON_CreateObject();
            cJSON_AddStringToObject(pAnimation, "id", text(animations[i], "id"));
            cJSON_AddStringToObject(pAnimation, "name", animation.name);
            cJSON_AddStringToObject(pAnimation, "path", animation.path);
            cJSON_AddStringToObject(pAnimation, "skeleton", text(skeletons[animation.skeleton], "id"));
            cJSON_AddItemToArray(pAnimations, pAnimation);
        }
    }
    if (!buildNodes(data, { meshes.GetData(), meshes.Size() }, { skeletons.GetData(), skeletons.Size() },
                    { animations.GetData(), animations.Size() }))
    {
        return false;
    }
    return validate();
}

static Matrix4 nodeWorldMatrix(const cgltf_node* pNode)
{
    if (!pNode)
    {
        return Matrix4::identity();
    }
    float m[16];
    cgltf_node_transform_world(pNode, m);
    return Matrix4(Vector4(m[0], m[1], m[2], m[3]), Vector4(m[4], m[5], m[6], m[7]), Vector4(m[8], m[9], m[10], m[11]),
                   Vector4(m[12], m[13], m[14], m[15]));
}

// Stores the bind pose of pSource relative to an emitted ancestor, for nodes whose skeleton parents leave the hierarchy.
static void addRelativeMatrix(cJSON* pNode, const cgltf_node* pSource, const cgltf_node* pAncestor)
{
    const Matrix4 relative = inverse(nodeWorldMatrix(pAncestor)) * nodeWorldMatrix(pSource);
    float         values[16];
    for (uint32_t column = 0; column < 4; ++column)
    {
        for (uint32_t row = 0; row < 4; ++row)
        {
            values[column * 4 + row] = row == 3 ? (column == 3 ? 1.0f : 0.0f) : relative.getElem((int)column, (int)row);
        }
    }
    addFloats(pNode, "matrix", values, 16);
}

static void addIdentityTransform(cJSON* pNode)
{
    const float translation[3] = {}, rotation[4] = { 0, 0, 0, 1 }, scale[3] = { 1, 1, 1 };
    addFloats(pNode, "translation", translation, 3);
    addFloats(pNode, "rotation", rotation, 4);
    addFloats(pNode, "scale", scale, 3);
}

// Skeleton nodes leave the hierarchy, because the animation of the skeleton entity drives them. Each skeleton becomes
// one entity below the parent of its roots, and skinned meshes attach to it with an identity transform, as glTF
// ignores the transform of a skinned mesh node.
bool SceneAssetCooker::buildNodes(const cgltf_data& data, hz::Span<cJSON*> meshes, hz::Span<cJSON*> skeletons, hz::Span<cJSON*> animations)
{
    cJSON*              pNodes = cJSON_AddArrayToObject(pAsset, "nodes");
    hz::Array<uint32_t> selected((uint32_t)data.nodes_count);
    hz::Array<uint8_t>  visited((uint32_t)data.nodes_count);
    hz::Array<uint32_t> stack;
    for (uint32_t& index : selected)
        index = UINT32_MAX;
    const cgltf_scene* pScene = data.scene ? data.scene : (data.scenes_count ? &data.scenes[0] : nullptr);
    if (pScene)
    {
        for (cgltf_size i = pScene->nodes_count; i > 0; --i)
            stack.PushBack((uint32_t)(pScene->nodes[i - 1] - data.nodes));
    }
    else
        for (uint32_t i = selected.Size(); i > 0; --i)
        {
            if (!data.nodes[i - 1].parent)
                stack.PushBack(i - 1);
        }
    uint32_t nodeCount = 0;
    uint32_t attachments = 0;
    while (!stack.Empty())
    {
        const uint32_t index = stack[stack.Size() - 1];
        stack.PopBack();
        if (visited[index])
        {
            LOGF(Error, "Scene node occurs more than once in the selected hierarchy");
            return false;
        }
        visited[index] = 1;
        const cgltf_node& node = data.nodes[index];
        for (cgltf_size i = node.children_count; i > 0; --i)
        {
            stack.PushBack((uint32_t)(node.children[i - 1] - data.nodes));
        }
        // A skinned mesh without children moves below its skeleton entity; one with children keeps its place for them.
        if (skeletonCooker.isSkeletonNode(index) || (node.skin && node.mesh && !node.children_count))
        {
            continue;
        }
        const cgltf_node* pAncestor = node.parent;
        while (pAncestor && selected[(uint32_t)(pAncestor - data.nodes)] == UINT32_MAX)
        {
            pAncestor = pAncestor->parent;
        }
        selected[index] = nodeCount++;
        cJSON* pNode = cJSON_CreateObject();
        cJSON_AddStringToObject(pNode, "name", node.name ? node.name : "");
        cJSON_AddNumberToObject(pNode, "parent", pAncestor ? (double)selected[(uint32_t)(pAncestor - data.nodes)] : -1.0);
        if (pAncestor != node.parent)
        {
            ++attachments;
            addRelativeMatrix(pNode, &node, pAncestor);
        }
        else if (node.has_matrix)
        {
            addFloats(pNode, "matrix", node.matrix, 16);
        }
        else
        {
            addFloats(pNode, "translation", node.translation, 3);
            addFloats(pNode, "rotation", node.rotation, 4);
            addFloats(pNode, "scale", node.scale, 3);
        }
        if (node.mesh && !node.skin)
        {
            cJSON_AddStringToObject(pNode, "mesh", text(meshes.Data[(uint32_t)(node.mesh - data.meshes)], "id"));
        }
        cJSON_AddItemToArray(pNodes, pNode);
    }
    if (attachments)
    {
        LOGF(Warning, "%u nodes below skeleton joints keep their bind pose; they do not follow the animation", attachments);
    }

    hz::Array<uint32_t> skeletonEntities(skeletons.Count);
    for (uint32_t k = 0; k < skeletons.Count; ++k)
    {
        const CookedSkeleton& skeleton = skeletonCooker.skeletons[k];
        const cgltf_node*     pParent = skeleton.parent == UINT32_MAX ? nullptr : &data.nodes[skeleton.parent];
        const cgltf_node*     pAncestor = pParent;
        while (pAncestor && selected[(uint32_t)(pAncestor - data.nodes)] == UINT32_MAX)
        {
            pAncestor = pAncestor->parent;
        }
        skeletonEntities[k] = nodeCount++;
        cJSON* pNode = cJSON_CreateObject();
        cJSON_AddStringToObject(pNode, "name", skeleton.name);
        cJSON_AddNumberToObject(pNode, "parent", pAncestor ? (double)selected[(uint32_t)(pAncestor - data.nodes)] : -1.0);
        if (pAncestor != pParent)
        {
            addRelativeMatrix(pNode, pParent, pAncestor);
        }
        else
        {
            addIdentityTransform(pNode);
        }
        cJSON_AddStringToObject(pNode, "skeleton", text(skeletons.Data[k], "id"));
        for (uint32_t i = 0; i < animations.Count; ++i)
        {
            if (skeletonCooker.animations[i].skeleton == k)
            {
                cJSON_AddStringToObject(pNode, "animation", text(animations.Data[i], "id"));
                break;
            }
        }
        cJSON_AddItemToArray(pNodes, pNode);
    }
    for (uint32_t index = 0; index < (uint32_t)data.nodes_count; ++index)
    {
        const cgltf_node& node = data.nodes[index];
        if (!visited[index] || !node.skin || !node.mesh)
        {
            continue;
        }
        uint32_t       skin = 0;
        const uint32_t skeleton = skeletonCooker.findSkeleton((uint32_t)(node.skin - data.skins), skin);
        ASSERT(skeleton != UINT32_MAX);
        cJSON* pNode = cJSON_CreateObject();
        cJSON_AddStringToObject(pNode, "name", node.name ? node.name : "");
        cJSON_AddNumberToObject(pNode, "parent", skeletonEntities[skeleton]);
        addIdentityTransform(pNode);
        cJSON_AddStringToObject(pNode, "mesh", text(meshes.Data[(uint32_t)(node.mesh - data.meshes)], "id"));
        cJSON_AddNumberToObject(pNode, "skin", skin);
        cJSON_AddItemToArray(pNodes, pNode);
    }
    return true;
}

bool SceneAssetCooker::validate() const
{
    char* pMetadataJson = cJSON_Print(pMetadata);
    if (!pMetadataJson)
        return false;
    addHash(pAsset, "identityHash", cookHash(pMetadataJson, strlen(pMetadataJson)));
    cJSON_free(pMetadataJson);
    char* pJson = cJSON_PrintUnformatted(pAsset);
    if (!pJson)
        return false;
    hz::SceneAsset asset;
    const bool     valid = asset.Parse({ pJson, (uint32_t)strlen(pJson) });
    cJSON_free(pJson);
    return valid;
}

bool SceneAssetCooker::write() const
{
    ASSERT(pMetadata && pAsset);
    hz::Array<char> catalog;
    if (!buildCatalog(pAsset, assetFile, catalog))
        return false;
    for (const cJSON* pEntry = pTextureMetadata->child; pEntry; pEntry = pEntry->next)
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(pEntry, "dirty")) &&
            !writeJson(sourceDirectory, text(pEntry, "path"), cJSON_GetObjectItemCaseSensitive(pEntry, "document")))
            return false;
    if (!writeJson(sourceDirectory, metadataFile, pMetadata))
        return false;
    // Animation files go out before the scene and catalog that reference them.
    for (const CookedSkeleton& skeleton : skeletonCooker.skeletons)
    {
        if (!writeBytes(outputDirectory, skeleton.path, skeleton.file.GetData(), skeleton.file.Size()))
        {
            return false;
        }
    }
    for (const CookedAnimation& animation : skeletonCooker.animations)
    {
        if (!writeBytes(outputDirectory, animation.path, animation.file.GetData(), animation.file.Size()))
        {
            return false;
        }
    }
    return writeJson(outputDirectory, assetFile, pAsset) && writeBytes(outputDirectory, catalogFile, catalog.GetData(), catalog.Size() - 1);
}
