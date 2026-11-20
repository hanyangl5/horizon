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
    Array<SceneAssetSkeleton>  skeletons;
    Array<SceneAssetAnimation> animations;
};

enum class AssetKind
{
    Scene,
    Texture,
    Material,
    Mesh,
    Skeleton,
    Animation,
};

struct AssetEntry
{
    AssetID   key;
    AssetKind value;
};

class SceneAssetReader
{
public:
    explicit SceneAssetReader(SceneAssetData& DataValue): data(DataValue) {}
    ~SceneAssetReader()
    {
        cJSON_Delete(pRoot);
        hmfree(pAssets);
    }
    SceneAssetReader(const SceneAssetReader&) = delete;
    SceneAssetReader& operator=(const SceneAssetReader&) = delete;

    bool read(Span<char> Json);

private:
    bool readTextures(const cJSON* ArrayPointer);
    bool readMaterials(const cJSON* ArrayPointer);
    bool readMeshes(const cJSON* ArrayPointer);
    bool readNodes(const cJSON* ArrayPointer);
    bool readSkeletons(const cJSON* ArrayPointer);
    bool readAnimations(const cJSON* ArrayPointer);
    bool readID(const cJSON* Object, AssetID& Id, AssetKind Kind);
    bool readReference(const cJSON* Object, const char* Field, AssetID& Id, AssetKind Kind, bool Optional = false);
    bool readString(const cJSON* Object, const char* Field, const char*& OutputPointer, bool PathValue = false);

    SceneAssetData& data;
    cJSON*          pRoot = nullptr;
    AssetEntry*     pAssets = nullptr;
};

static bool InvalidField(const char* Field)
{
    LOGF(Error, "Invalid cooked SceneAsset field '%s'", Field);
    return false;
}

static const cJSON* FindSceneField(const cJSON* Object, const char* Name) { return cJSON_GetObjectItemCaseSensitive(Object, Name); }

static bool ReadFloat(const cJSON* ValueData, float& OutputValue, float Minimum = -FLT_MAX, float Maximum = FLT_MAX)
{
    if (!cJSON_IsNumber(ValueData) || !isfinite(ValueData->valuedouble) || ValueData->valuedouble < Minimum || ValueData->valuedouble > Maximum)
        return false;
    OutputValue = (float)ValueData->valuedouble;
    return true;
}

static bool ReadFloats(const cJSON* Object, const char* Name, float* OutputPointer, uint32_t Count, float Minimum = -FLT_MAX,
                       float Maximum = FLT_MAX)
{
    const cJSON* ArrayPointer = FindSceneField(Object, Name);
    if (!cJSON_IsArray(ArrayPointer) || cJSON_GetArraySize(ArrayPointer) != (int)Count)
    {
        return InvalidField(Name);
    }
    uint32_t IndexValue = 0;
    for (const cJSON* ValueData = ArrayPointer->child; ValueData; ValueData = ValueData->next)
        if (!ReadFloat(ValueData, OutputPointer[IndexValue++], Minimum, Maximum))
            return InvalidField(Name);
    return true;
}

static bool ReadInteger(const cJSON* ValueData, double Minimum, double Maximum)
{
    return cJSON_IsNumber(ValueData) && isfinite(ValueData->valuedouble) && ValueData->valuedouble >= Minimum && ValueData->valuedouble <= Maximum &&
           floor(ValueData->valuedouble) == ValueData->valuedouble;
}

static uint32_t StringSize(const cJSON* Object, const char* Field)
{
    const cJSON* ValueData = FindSceneField(Object, Field);
    return cJSON_IsString(ValueData) ? (uint32_t)strlen(ValueData->valuestring) + 1 : 0;
}

static int CompareSubmeshIDs(const void* LeftValue, const void* RightValue)
{
    const SubmeshID AValue = *(const SubmeshID*)LeftValue;
    const SubmeshID BValue = *(const SubmeshID*)RightValue;
    return (AValue > BValue) - (AValue < BValue);
}

static bool IsRelativePath(const char* PathPointer)
{
    if (!*PathPointer || *PathPointer == '/' || strchr(PathPointer, '\\') || strchr(PathPointer, ':'))
        return false;
    const char* Segment = PathPointer;
    for (const char* CursorPointer = PathPointer;; ++CursorPointer)
        if (*CursorPointer == '/' || !*CursorPointer)
        {
            if (CursorPointer == Segment || (CursorPointer - Segment == 1 && *Segment == '.') ||
                (CursorPointer - Segment == 2 && Segment[0] == '.' && Segment[1] == '.'))
                return false;
            if (!*CursorPointer)
                return true;
            Segment = CursorPointer + 1;
        }
}

bool SceneAssetReader::readString(const cJSON* Object, const char* Field, const char*& OutputPointer, bool PathValue)
{
    const cJSON* ValueData = FindSceneField(Object, Field);
    if (!cJSON_IsString(ValueData) || (PathValue && !IsRelativePath(ValueData->valuestring)))
        return InvalidField(Field);
    const uint32_t Offset = data.strings.Size();
    const uint32_t Size = (uint32_t)strlen(ValueData->valuestring) + 1;
    data.strings.Resize(Offset + Size);
    memcpy(data.strings.GetData() + Offset, ValueData->valuestring, Size);
    OutputPointer = data.strings.GetData() + Offset;
    return true;
}

bool SceneAssetReader::readID(const cJSON* Object, AssetID& Id, AssetKind Kind)
{
    const cJSON* ValueData = FindSceneField(Object, "id");
    if (!cJSON_IsString(ValueData) || !Id.Parse(ValueData->valuestring) || !Id.IsValid() || hmgetp_null(pAssets, Id))
    {
        return InvalidField("id (invalid or duplicate AssetID)");
    }
    const AssetEntry EntryValue = { .key = Id, .value = Kind };
    hmputs(pAssets, EntryValue);
    return true;
}

bool SceneAssetReader::readReference(const cJSON* Object, const char* Field, AssetID& Id, AssetKind Kind, bool Optional)
{
    const cJSON* ValueData = FindSceneField(Object, Field);
    if (cJSON_IsNull(ValueData) || (!ValueData && Optional))
        return true;
    if (!cJSON_IsString(ValueData) || !Id.Parse(ValueData->valuestring) || !Id.IsValid())
    {
        return InvalidField(Field);
    }
    const AssetEntry* EntryPointer = hmgetp_null(pAssets, Id);
    return (EntryPointer && EntryPointer->value == Kind) || InvalidField(Field);
}

bool SceneAssetReader::readTextures(const cJSON* ArrayPointer)
{
    if (!cJSON_IsArray(ArrayPointer))
        return InvalidField("textures");
    data.textures.Reserve((uint32_t)cJSON_GetArraySize(ArrayPointer));
    for (const cJSON* Item = ArrayPointer->child; Item; Item = Item->next)
    {
        SceneAssetTexture TextureValue;
        if (!readID(Item, TextureValue.id, AssetKind::Texture) || !readString(Item, "path", TextureValue.pPath, true) ||
            !readString(Item, "source", TextureValue.pSource, true))
            return false;
        const cJSON* Srgb = FindSceneField(Item, "srgb");
        if (!cJSON_IsBool(Srgb))
            return InvalidField("srgb");
        TextureValue.srgb = cJSON_IsTrue(Srgb);
        const cJSON* Cooked = FindSceneField(Item, "cooked");
        if (!cJSON_IsBool(Cooked))
            return InvalidField("cooked");
        TextureValue.cooked = cJSON_IsTrue(Cooked);
        data.textures.PushBack(TextureValue);
    }
    return true;
}

bool SceneAssetReader::readMaterials(const cJSON* ArrayPointer)
{
    if (!cJSON_IsArray(ArrayPointer))
        return InvalidField("materials");
    data.materials.Reserve((uint32_t)cJSON_GetArraySize(ArrayPointer));
    for (const cJSON* Item = ArrayPointer->child; Item; Item = Item->next)
    {
        SceneAssetMaterial MaterialValue;
        if (!readID(Item, MaterialValue.id, AssetKind::Material) || !readString(Item, "name", MaterialValue.Name) ||
            !readReference(Item, "baseColorTexture", MaterialValue.baseColorTexture, AssetKind::Texture) ||
            !readReference(Item, "normalTexture", MaterialValue.normalTexture, AssetKind::Texture) ||
            !readReference(Item, "metallicRoughnessTexture", MaterialValue.metallicRoughnessTexture, AssetKind::Texture) ||
            !readReference(Item, "emissiveTexture", MaterialValue.emissiveTexture, AssetKind::Texture) ||
            !ReadFloats(Item, "baseColorFactor", MaterialValue.baseColorFactor, 4, 0.0f, 1.0f) ||
            !ReadFloats(Item, "emissiveFactor", MaterialValue.emissiveFactor, 3, 0.0f))
        {
            return false;
        }
        if (!ReadFloat(FindSceneField(Item, "metallicFactor"), MaterialValue.metallicFactor, 0.0f, 1.0f) ||
            !ReadFloat(FindSceneField(Item, "roughnessFactor"), MaterialValue.roughnessFactor, 0.0f, 1.0f) ||
            !ReadFloat(FindSceneField(Item, "alphaCutoff"), MaterialValue.alphaCutoff, 0.0f) || !ReadInteger(FindSceneField(Item, "alphaMode"), 0, 2) ||
            !cJSON_IsBool(FindSceneField(Item, "doubleSided")))
            return InvalidField("material factors/alpha mode");
        MaterialValue.alphaMode = (MaterialAlphaMode)FindSceneField(Item, "alphaMode")->valueint;
        MaterialValue.doubleSided = cJSON_IsTrue(FindSceneField(Item, "doubleSided"));
        // Optional, so assets cooked before tags existed stay valid.
        const cJSON* Tags = FindSceneField(Item, "tags");
        if (Tags && !ReadInteger(Tags, 0, UINT32_MAX))
        {
            return InvalidField("material tags");
        }
        MaterialValue.tags = Tags ? (uint32_t)Tags->valuedouble : 0;
        data.materials.PushBack(MaterialValue);
    }
    return true;
}

bool SceneAssetReader::readMeshes(const cJSON* ArrayPointer)
{
    if (!cJSON_IsArray(ArrayPointer))
        return InvalidField("meshes");
    uint32_t SubmeshCount = 0;
    for (const cJSON* Item = ArrayPointer->child; Item; Item = Item->next)
    {
        if (!cJSON_IsArray(FindSceneField(Item, "submeshes")) || !FindSceneField(Item, "submeshes")->child)
            return InvalidField("submeshes");
        SubmeshCount += (uint32_t)cJSON_GetArraySize(FindSceneField(Item, "submeshes"));
    }
    data.submeshes.Reserve(SubmeshCount);
    data.meshes.Reserve((uint32_t)cJSON_GetArraySize(ArrayPointer));
    Array<SubmeshID> Ids;
    for (const cJSON* Item = ArrayPointer->child; Item; Item = Item->next)
    {
        SceneAssetMesh MeshValue;
        if (!readID(Item, MeshValue.id, AssetKind::Mesh) || !readString(Item, "name", MeshValue.Name) ||
            !readString(Item, "geometry", MeshValue.pGeometry, true))
        {
            return false;
        }
        const uint32_t First = data.submeshes.Size();
        Ids.Clear();
        for (const cJSON* SubmeshPointer = FindSceneField(Item, "submeshes")->child; SubmeshPointer; SubmeshPointer = SubmeshPointer->next)
        {
            const cJSON* ID = FindSceneField(SubmeshPointer, "id");
            if (!cJSON_IsString(ID) || strlen(ID->valuestring) != 16 || strspn(ID->valuestring, "0123456789abcdefABCDEF") != 16 ||
                !ReadInteger(FindSceneField(SubmeshPointer, "draw"), 0, UINT32_MAX))
                return InvalidField("submesh id/Draw");
            SceneAssetSubmesh SubmeshValue = { .id = strtoull(ID->valuestring, nullptr, 16),
                                          .Draw = (uint32_t)FindSceneField(SubmeshPointer, "draw")->valuedouble };
            if (!SubmeshValue.id)
                return InvalidField("submesh id");
            if (!readReference(SubmeshPointer, "material", SubmeshValue.material, AssetKind::Material))
                return false;
            if (!SubmeshValue.material.IsValid())
            {
                return InvalidField("submesh material");
            }
            data.submeshes.PushBack(SubmeshValue);
            Ids.PushBack(SubmeshValue.id);
        }
        qsort(Ids.GetData(), Ids.Size(), sizeof(SubmeshID), CompareSubmeshIDs);
        for (uint32_t I = 1; I < Ids.Size(); ++I)
        {
            if (Ids[I - 1] == Ids[I])
                return InvalidField("submeshes (duplicate SubmeshID within mesh)");
        }
        MeshValue.submeshes = { data.submeshes.GetData() + First, data.submeshes.Size() - First };
        data.meshes.PushBack(MeshValue);
    }
    return true;
}

bool SceneAssetReader::readSkeletons(const cJSON* ArrayPointer)
{
    // Optional: scenes without skins have no table.
    if (!ArrayPointer)
        return true;
    if (!cJSON_IsArray(ArrayPointer))
        return InvalidField("skeletons");
    data.skeletons.Reserve((uint32_t)cJSON_GetArraySize(ArrayPointer));
    for (const cJSON* Item = ArrayPointer->child; Item; Item = Item->next)
    {
        SceneAssetSkeleton SkeletonValue;
        if (!readID(Item, SkeletonValue.id, AssetKind::Skeleton) || !readString(Item, "name", SkeletonValue.Name) ||
            !readString(Item, "path", SkeletonValue.pPath, true))
            return false;
        data.skeletons.PushBack(SkeletonValue);
    }
    return true;
}

bool SceneAssetReader::readAnimations(const cJSON* ArrayPointer)
{
    if (!ArrayPointer)
        return true;
    if (!cJSON_IsArray(ArrayPointer))
        return InvalidField("animations");
    data.animations.Reserve((uint32_t)cJSON_GetArraySize(ArrayPointer));
    for (const cJSON* Item = ArrayPointer->child; Item; Item = Item->next)
    {
        SceneAssetAnimation AnimationValue;
        if (!readID(Item, AnimationValue.id, AssetKind::Animation) || !readString(Item, "name", AnimationValue.Name) ||
            !readString(Item, "path", AnimationValue.pPath, true) || !readReference(Item, "skeleton", AnimationValue.skeleton, AssetKind::Skeleton))
            return false;
        if (!AnimationValue.skeleton.IsValid())
            return InvalidField("animations.skeleton");
        data.animations.PushBack(AnimationValue);
    }
    return true;
}

static bool ValidateHierarchy(Span<SceneAssetNode> Nodes)
{
    Array<uint8_t> Visited(Nodes.Count);
    for (uint32_t I = 0; I < Nodes.Count; ++I)
    {
        uint32_t CursorValue = I;
        while (CursorValue != UINT32_MAX && Visited[CursorValue] == 0)
        {
            Visited[CursorValue] = 1;
            CursorValue = Nodes.Data[CursorValue].parent;
        }
        if (CursorValue != UINT32_MAX && Visited[CursorValue] == 1)
            return InvalidField("nodes.parent (hierarchy cycle)");
        CursorValue = I;
        while (CursorValue != UINT32_MAX && Visited[CursorValue] == 1)
        {
            Visited[CursorValue] = 2;
            CursorValue = Nodes.Data[CursorValue].parent;
        }
    }
    return true;
}

bool SceneAssetReader::readNodes(const cJSON* ArrayPointer)
{
    if (!cJSON_IsArray(ArrayPointer))
        return InvalidField("nodes");
    const uint32_t Count = (uint32_t)cJSON_GetArraySize(ArrayPointer);
    data.nodes.Reserve(Count);
    for (const cJSON* Item = ArrayPointer->child; Item; Item = Item->next)
    {
        SceneAssetNode NodeValue;
        if (!readString(Item, "name", NodeValue.Name) || !readReference(Item, "mesh", NodeValue.mesh, AssetKind::Mesh, true) ||
            !readReference(Item, "skeleton", NodeValue.skeleton, AssetKind::Skeleton, true) ||
            !readReference(Item, "animation", NodeValue.animation, AssetKind::Animation, true))
        {
            return false;
        }
        const cJSON* Skin = FindSceneField(Item, "skin");
        if (Skin)
        {
            if (!ReadInteger(Skin, 0, UINT16_MAX) || !NodeValue.mesh.IsValid())
                return InvalidField("nodes.skin (expected a skin index on a mesh node)");
            NodeValue.skin = (uint32_t)Skin->valuedouble;
        }
        if (NodeValue.animation.IsValid())
        {
            const SceneAssetAnimation* AnimationPointer = nullptr;
            for (const SceneAssetAnimation& AnimationValue : data.animations)
                if (AnimationValue.id == NodeValue.animation)
                    AnimationPointer = &AnimationValue;
            if (!NodeValue.skeleton.IsValid() || !AnimationPointer || AnimationPointer->skeleton != NodeValue.skeleton)
                return InvalidField("nodes.animation (expected a clip of the node's skeleton)");
        }
        if (!ReadInteger(FindSceneField(Item, "parent"), -1, (double)Count - 1))
        {
            return InvalidField("nodes.parent");
        }
        const double Parent = FindSceneField(Item, "parent")->valuedouble;
        NodeValue.parent = Parent < 0 ? UINT32_MAX : (uint32_t)Parent;
        NodeValue.hasMatrix = FindSceneField(Item, "matrix") != nullptr;
        if (NodeValue.hasMatrix)
        {
            float Matrix[16];
            if (FindSceneField(Item, "translation") || FindSceneField(Item, "rotation") || FindSceneField(Item, "scale"))
                return InvalidField("nodes (matrix and TRS are mutually exclusive)");
            if (!ReadFloats(Item, "matrix", Matrix, 16))
                return false;
            if (Matrix[3] != 0.0f || Matrix[7] != 0.0f || Matrix[11] != 0.0f || Matrix[15] != 1.0f)
                return InvalidField("nodes.matrix (expected affine matrix)");
            for (uint32_t Column = 0; Column < 4; ++Column)
                for (uint32_t Row = 0; Row < 4; ++Row)
                    NodeValue.matrix.setElem((int)Column, (int)Row, Matrix[Column * 4 + Row]);
        }
        else
        {
            float Translation[3], Rotation[4], Scale[3];
            if (!ReadFloats(Item, "translation", Translation, 3) || !ReadFloats(Item, "rotation", Rotation, 4) ||
                !ReadFloats(Item, "scale", Scale, 3))
                return false;
            const double Length = (double)Rotation[0] * Rotation[0] + (double)Rotation[1] * Rotation[1] +
                                  (double)Rotation[2] * Rotation[2] + (double)Rotation[3] * Rotation[3];
            if (fabs(Length - 1.0) > 0.001)
                return InvalidField("nodes.rotation (expected unit quaternion)");
            NodeValue.transform.translation = Vector3(Translation[0], Translation[1], Translation[2]);
            NodeValue.transform.rotation = Quat(Rotation[0], Rotation[1], Rotation[2], Rotation[3]);
            NodeValue.transform.scale = Vector3(Scale[0], Scale[1], Scale[2]);
        }
        data.nodes.PushBack(NodeValue);
    }
    for (const SceneAssetNode& NodeValue : data.nodes)
        if (NodeValue.skin != UINT32_MAX && (NodeValue.parent == UINT32_MAX || !data.nodes[NodeValue.parent].skeleton.IsValid()))
            return InvalidField("nodes.skin (the parent node must own a skeleton)");
    return ValidateHierarchy({ data.nodes.GetData(), data.nodes.Size() });
}

bool SceneAssetReader::read(Span<char> Json)
{
    const char* End = nullptr;
    if (Json.Data && Json.Count && Json.Count <= 64 * 1024 * 1024)
    {
        pRoot = cJSON_ParseWithLengthOpts(Json.Data, Json.Count, &End, false);
    }
    if (pRoot)
    {
        while (End < Json.Data + Json.Count && isspace((unsigned char)*End))
        {
            ++End;
        }
        // Accept both byte spans and string literals, which include their final null.
        if (End < Json.Data + Json.Count && !*End && End + 1 == Json.Data + Json.Count)
        {
            ++End;
        }
    }
    if (!cJSON_IsObject(pRoot) || End != Json.Data + Json.Count)
    {
        return InvalidField("JSON (expected one object, at most 64 MiB)");
    }
    const cJSON* FormatPointer = FindSceneField(pRoot, "format");
    const cJSON* Version = FindSceneField(pRoot, "version");
    if (!cJSON_IsString(FormatPointer) || strcmp(FormatPointer->valuestring, "Horizon.SceneAsset") != 0 || !ReadInteger(Version, 1, 1))
        return InvalidField("format/version");
    uint32_t    StringBytes = StringSize(pRoot, "source") + StringSize(pRoot, "contentHash");
    const char* Tables[] = { "textures", "materials", "meshes", "nodes", "skeletons", "animations" };
    for (const char* Table : Tables)
        for (const cJSON* Item = FindSceneField(pRoot, Table) ? FindSceneField(pRoot, Table)->child : nullptr; Item; Item = Item->next)
            StringBytes +=
                StringSize(Item, "name") + StringSize(Item, "path") + StringSize(Item, "geometry") + StringSize(Item, "source");
    data.strings.Reserve(StringBytes);
    if (!readID(pRoot, data.id, AssetKind::Scene) || !readString(pRoot, "source", data.pSource, true) ||
        !readString(pRoot, "contentHash", data.pContentHash))
        return false;
    const cJSON* IdentityHash = FindSceneField(pRoot, "identityHash");
    if (strncmp(data.pContentHash, "fnv1a64:", 8) != 0 || strlen(data.pContentHash) != 24 ||
        strspn(data.pContentHash + 8, "0123456789abcdefABCDEF") != 16 || !cJSON_IsString(IdentityHash) ||
        strlen(IdentityHash->valuestring) != 16 || strspn(IdentityHash->valuestring, "0123456789abcdefABCDEF") != 16)
        return InvalidField("contentHash/identityHash");
    return readTextures(FindSceneField(pRoot, "textures")) && readMaterials(FindSceneField(pRoot, "materials")) && readMeshes(FindSceneField(pRoot, "meshes")) &&
           readSkeletons(FindSceneField(pRoot, "skeletons")) && readAnimations(FindSceneField(pRoot, "animations")) && readNodes(FindSceneField(pRoot, "nodes"));
}

SceneAsset::~SceneAsset() { MemDelete(Data); }

SceneAsset::SceneAsset(SceneAsset&& Other) noexcept: Data(Other.Data) { Other.Data = nullptr; }

SceneAsset& SceneAsset::operator=(SceneAsset&& Other) noexcept
{
    if (this != &Other)
    {
        MemDelete(Data);
        Data = Other.Data;
        Other.Data = nullptr;
    }
    return *this;
}

bool SceneAsset::Parse(Span<char> Json)
{
    SceneAssetData*  Candidate = MemNew(SceneAssetData);
    SceneAssetReader Reader(*Candidate);
    if (!Reader.read(Json))
    {
        MemDelete(Candidate);
        return false;
    }
    MemDelete(Data);
    Data = Candidate;
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
Span<SceneAssetSkeleton> SceneAsset::GetSkeletons() const
{
    return Data ? Span<SceneAssetSkeleton>(Data->skeletons.GetData(), Data->skeletons.Size()) : Span<SceneAssetSkeleton>{};
}
Span<SceneAssetAnimation> SceneAsset::GetAnimations() const
{
    return Data ? Span<SceneAssetAnimation>(Data->animations.GetData(), Data->animations.Size()) : Span<SceneAssetAnimation>{};
}
static void CopyName(Name& NameValue, const char* Text)
{
    size_t Length = 0;
    while (Length < sizeof(NameValue.value) - 1 && Text[Length])
        ++Length;
    while (Length && ((uint8_t)Text[Length] & 0xc0) == 0x80)
        --Length;
    memcpy(NameValue.value, Text, Length);
    NameValue.value[Length] = 0;
}

Entity SceneAsset::Instantiate(World& WorldData, Entity Parent) const
{
    ASSERT(Data && !WorldData.IsDeferred() && (!Parent.IsValid() || WorldData.IsAlive(Parent)));
    const Entity Root = WorldData.CreatePersistentEntity();
    if (!Root.IsValid())
    {
        return {};
    }
    Array<Entity> Entities;
    Entities.Reserve(Data->nodes.Size());
    {
        DeferredChanges Changes = WorldData.Defer();
        Name            NameValue;
        CopyName(NameValue, Data->pSource);
        bool Copied = WorldData.SetComponent(Root, SceneTypes::name, &NameValue);
        for (const SceneAssetNode& NodeValue : Data->nodes)
        {
            const Entity EntityHandle = WorldData.CreatePersistentEntity();
            if (!EntityHandle.IsValid())
            {
                break;
            }
            Entities.PushBack(EntityHandle);
            CopyName(NameValue, NodeValue.Name);
            Copied &= WorldData.SetComponent(EntityHandle, SceneTypes::name, &NameValue);
            Copied &= NodeValue.hasMatrix ? WorldData.SetComponent(EntityHandle, SceneTypes::localMatrix, &NodeValue.matrix)
                                     : WorldData.SetComponent(EntityHandle, SceneTypes::localTransform, &NodeValue.transform);
            if (NodeValue.mesh.IsValid())
            {
                const MeshRenderer RendererValue = { .mesh = NodeValue.mesh };
                Copied &= WorldData.SetComponent(EntityHandle, SceneTypes::meshRenderer, &RendererValue);
            }
            if (NodeValue.skin != UINT32_MAX)
            {
                const SkinnedMesh Skinned = { .skin = NodeValue.skin };
                Copied &= WorldData.SetComponent(EntityHandle, SceneTypes::skinnedMesh, &Skinned);
            }
            if (NodeValue.skeleton.IsValid())
            {
                const Skeleton SkeletonValue = { .asset = NodeValue.skeleton };
                Copied &= WorldData.SetComponent(EntityHandle, SceneTypes::skeleton, &SkeletonValue);
            }
            if (NodeValue.animation.IsValid())
            {
                const AnimationPlayer Player = { .clip = NodeValue.animation };
                Copied &= WorldData.SetComponent(EntityHandle, SceneTypes::animationPlayer, &Player);
            }
        }
        ASSERT(Copied);
    }
    if (Entities.Size() != Data->nodes.Size())
    {
        for (Entity EntityHandle : Entities)
            WorldData.DestroyEntity(EntityHandle);
        WorldData.DestroyEntity(Root);
        return {};
    }
    if (Parent.IsValid())
    {
        WorldData.AttachParent(Root, Parent);
    }
    for (uint32_t I = 0; I < Entities.Size(); ++I)
    {
        WorldData.AttachParent(Entities[I], Data->nodes[I].parent == UINT32_MAX ? Root : Entities[Data->nodes[I].parent]);
    }
    return Root;
}
} // namespace hz
