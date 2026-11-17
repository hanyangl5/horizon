#pragma once

#include <gtest/gtest.h>
#include <filesystem>
#include <vector>

#include "Scene/AssetManager.h"
#include "Scene/SceneAssetLoader.h"
#include "Scene/SceneAsset.h"
#include "Scene/TextureAsset.h"
#include "Scene/AssetLoaders.h"
#include "Scene/MeshAsset.h"
#include "Scene/MaterialAsset.h"
#include "Scene/WorldAssetBindings.h"
#include "Scene/RenderScene.h"
#include "Scene/GpuScene.h"
#include "Scene/WorldArchive.h"
#include "Core/IUniquePtr.h"
#include "Resources/IResourceLoader.h"
#include "Graphics/RenderContext.h"
#include "Core/IThread.h"
#include "Core/ITime.h"
#include "Core/IContainer.h"

inline hz::AssetID assetID(uint32_t value) { return { .high = 1, .low = value }; }

class TestAssetLoader
{
public:
    struct Payload
    {
        uint32_t id;
    };
    struct Job
    {
        Payload* pPayload;
        bool     ready;
        bool     failed = false;
    };

    hz::AssetLoader GetLoader()
    {
        return { .UserData = this, .pBegin = begin, .pPoll = poll, .pDestroyRequest = destroyRequest, .pDestroyPayload = destroyPayload };
    }

    hz::AssetLoader getGpuLoader()
    {
        hz::AssetLoader loader = GetLoader();
        loader.pUpload = Upload;
        loader.pPollUpload = pollUpload;
        loader.pWaitUpload = waitUpload;
        return loader;
    }
    static void Upload(void*, const hz::AssetManager& assetManager, hz::Span<hz::AssetHandle> dependencies, void* User)
    {
        ++((TestAssetLoader*)User)->uploads;
        for (hz::AssetHandle dependency : dependencies)
            EXPECT_EQ(assetManager.GetState(dependency), hz::AssetState::Ready);
    }
    static hz::AssetLoadStatus pollUpload(void*, void* User)
    {
        TestAssetLoader& loader = *(TestAssetLoader*)User;
        if (!loader.uploadReady)
            return hz::AssetLoadStatus::Pending;
        return loader.uploadFailed ? hz::AssetLoadStatus::Failed : hz::AssetLoadStatus::Succeeded;
    }
    static void waitUpload(void*, void* User) { ++((TestAssetLoader*)User)->uploadWaits; }

    static void* begin(const hz::AssetCatalogEntry& entry, hz::AssetHandle, void* User)
    {
        TestAssetLoader& loader = *(TestAssetLoader*)User;
        const uint32_t   id = (uint32_t)entry.id.low;
        ++loader.begun;
        if (loader.failBegin == id)
            return nullptr;
        Payload* pPayload = MemNew(Payload);
        pPayload->id = id;
        Job* pJob = MemNew(Job);
        *pJob = { .pPayload = pPayload, .ready = loader.ready };
        loader.jobs[id] = pJob;
        return pJob;
    }

    static hz::AssetLoadResult poll(void* pRequest, void*)
    {
        Job& job = *(Job*)pRequest;
        if (!job.ready)
            return {};
        const hz::AssetLoadResult result = { .status = job.failed ? hz::AssetLoadStatus::Failed : hz::AssetLoadStatus::Succeeded,
                                             .pPayload = job.pPayload };
        job.pPayload = nullptr;
        return result;
    }

    static void destroyRequest(void* pRequest, void* User)
    {
        TestAssetLoader& loader = *(TestAssetLoader*)User;
        Job*             pJob = (Job*)pRequest;
        ++loader.requestsDestroyed;
        if (pJob->pPayload)
            destroyPayload(pJob->pPayload, User);
        MemDelete(pJob);
    }

    static void destroyPayload(void* pPayload, void* User)
    {
        TestAssetLoader& loader = *(TestAssetLoader*)User;
        Payload*         pValue = (Payload*)pPayload;
        loader.destroyed.push_back(pValue->id);
        MemDelete(pValue);
    }

    Job*                  jobs[2048] = {};
    std::vector<uint32_t> destroyed;
    uint32_t              begun = 0;
    uint32_t              requestsDestroyed = 0;
    uint32_t              failBegin = 0;
    bool                  ready = true;
    uint32_t              uploads = 0;
    uint32_t              uploadWaits = 0;
    bool                  uploadReady = false;
    bool                  uploadFailed = false;
};

class AssetManagerTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(InitMemoryAlloc(nullptr));
        initialStats = GetMemoryTrackingStats();
    }
    void TearDown() override
    {
        const MemoryTrackingStats stats = GetMemoryTrackingStats();
        if (stats.TrackingEnabled)
        {
            EXPECT_EQ(stats.LiveAllocationCount, initialStats.LiveAllocationCount);
            EXPECT_EQ(stats.LiveRequestedBytes, initialStats.LiveRequestedBytes);
        }
        ExitMemoryAlloc();
    }
    static void Add(hz::AssetCatalog& catalog, uint32_t id, hz::Span<hz::AssetID> dependencies = {})
    {
        ASSERT_TRUE(catalog.Add({ .id = assetID(id),
                                  .pSourcePath = "test.gltf",
                                  .location = { .pPath = "test.sceneasset.json" },
                                  .dependencies = dependencies }));
    }
    MemoryTrackingStats initialStats = {};
};

inline const char kEmptyScene[] = R"json({
    "format":"Horizon.SceneAsset", "version":1,
    "id":"00000000-0000-4000-8000-000000000001",
    "source":"scene.gltf", "contentHash":"fnv1a64:0123456789abcdef", "identityHash":"0123456789abcdef",
    "nodes":[], "meshes":[], "materials":[], "textures":[]
})json";

class SceneAssetLoaderTest: public AssetManagerTest
{
protected:
    void SetUp() override
    {
        AssetManagerTest::SetUp();
        FileSystemInitDesc desc = { .pAppName = "SceneAssetLoaderTest" };
        ASSERT_TRUE(InitFileSystem(&desc));
        char id[hz::kIDStringCapacity];
        hz::AssetID::create().ToString(id);
        directory = std::filesystem::temp_directory_path() / id;
        std::filesystem::create_directory(directory);
        fileSystem = *SystemFileIO;
        fileSystem.GetResourceMount = nullptr;
        SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, directory.string().c_str());
        FileStream stream = {};
        ASSERT_TRUE(OpenStreamFromPath(DirectoryOtherFiles, "scene.sceneasset.json", FileModeWrite, &stream));
        EXPECT_EQ(WriteToStream(&stream, kEmptyScene, sizeof(kEmptyScene) - 1), sizeof(kEmptyScene) - 1);
        CloseStream(&stream);
    }
    void TearDown() override
    {
        ExitFileSystem();
        std::filesystem::remove_all(directory);
        AssetManagerTest::TearDown();
    }
    static void Wait(hz::AssetManager& assetManager, hz::AssetHandle Handle)
    {
        for (uint32_t i = 0; i < 5000; ++i)
        {
            assetManager.Update();
            if (assetManager.GetState(Handle) == hz::AssetState::Ready || assetManager.GetState(Handle) == hz::AssetState::Failed)
                return;
            ThreadSleep(1);
        }
        FAIL() << "SceneAsset Load timed out";
    }
    std::filesystem::path directory;
    IFileSystem           fileSystem = {};
};

class TextureAssetLiveTest: public SceneAssetLoaderTest
{
protected:
    void writeTexture()
    {
        uint32_t dds[33] = {};
        dds[0] = 0x20534444;
        dds[1] = 124;
        dds[2] = 0x100f;
        dds[3] = dds[4] = 1;
        dds[5] = 4;
        dds[19] = 32;
        dds[20] = 0x41;
        dds[22] = 32;
        dds[23] = 0xff;
        dds[24] = 0xff00;
        dds[25] = 0xff0000;
        dds[26] = 0xff000000;
        dds[27] = 0x1000;
        dds[32] = 0xff4080ff;
        FileStream stream = {};
        ASSERT_TRUE(OpenStreamFromPath(DirectoryOtherFiles, "pixel.dds", FileModeWrite, &stream));
        EXPECT_EQ(WriteToStream(&stream, dds, sizeof(dds)), sizeof(dds));
        CloseStream(&stream);
    }
};

inline const char kTypedScene[] = R"json({
    "format":"Horizon.SceneAsset", "version":1,
    "id":"00000000-0000-4000-8000-000000000001",
    "source":"scene.gltf", "contentHash":"fnv1a64:0123456789abcdef", "identityHash":"0123456789abcdef",
    "textures":[{"id":"00000000-0000-4000-8000-000000000002", "path":"pixel.dds", "source":"pixel.png", "srgb":false, "cooked":true}],
    "materials":[{"id":"00000000-0000-4000-8000-000000000003", "name":"Shared",
        "baseColorTexture":"00000000-0000-4000-8000-000000000002", "normalTexture":null,
        "metallicRoughnessTexture":null, "emissiveTexture":null,
        "baseColorFactor":[1,0.5,0.25,1], "metallicFactor":0.2, "roughnessFactor":0.8,
        "emissiveFactor":[0,0,0], "alphaMode":1, "alphaCutoff":0.5, "doubleSided":true}],
    "meshes":[
        {"id":"00000000-0000-4000-8000-000000000004", "name":"First", "geometry":"scene.bin",
         "submeshes":[{"id":"8000000000000001", "draw":0, "material":"00000000-0000-4000-8000-000000000003"}]},
        {"id":"00000000-0000-4000-8000-000000000005", "name":"Second", "geometry":"scene.bin",
         "submeshes":[{"id":"8000000000000002", "draw":0, "material":"00000000-0000-4000-8000-000000000003"}]}
    ],
    "nodes":[
        {"name":"First", "parent":-1, "mesh":"00000000-0000-4000-8000-000000000004",
         "translation":[0,0,0], "rotation":[0,0,0,1], "scale":[1,1,1]},
        {"name":"Second", "parent":-1, "mesh":"00000000-0000-4000-8000-000000000005",
         "translation":[1,0,0], "rotation":[0,0,0,1], "scale":[1,1,1]}
    ]
})json";

inline const VertexLayout kAssetTestLayout = {
    .attribs = { { .semantic = SEMANTIC_POSITION, .Format = hz::Format::R32G32B32_SFLOAT, .binding = 2 } },
    .attribCount = 1,
};

class TypedAssetLiveTest: public TextureAssetLiveTest
{
protected:
    void writeScene(const char* text = kTypedScene, const char* path = "scene.sceneasset.json")
    {
        FileStream stream = {};
        ASSERT_TRUE(OpenStreamFromPath(DirectoryOtherFiles, path, FileModeWrite, &stream));
        EXPECT_EQ(WriteToStream(&stream, text, strlen(text)), strlen(text));
        CloseStream(&stream);
    }
    void writeGeometry(IndexType indexType = INDEX_TYPE_UINT16, bool rendererLayout = false)
    {
        Geometry geometry = {};
        geometry.drawArgCount = 1;
        geometry.indexCount = 3;
        geometry.vertexCount = 3;
        geometry.indexType = indexType;
        const IndirectDrawIndexArguments Draw = { .indexCount = 3, .instanceCount = 1 };
        const GeometryData               data = {};
        GeometryData::ShadowData         shadow = {};
        shadow.vertexStrides[SEMANTIC_POSITION] = 12;
        shadow.attributeCount[SEMANTIC_POSITION] = 3;
        if (rendererLayout)
        {
            shadow.vertexStrides[SEMANTIC_NORMAL] = shadow.vertexStrides[SEMANTIC_TEXCOORD0] = 4;
            shadow.attributeCount[SEMANTIC_NORMAL] = shadow.attributeCount[SEMANTIC_TEXCOORD0] = 3;
        }
        const uint32_t normals[] = { 0x80008000, 0x80008000, 0x80008000 }, uvs[] = { 0, 0, 0 };
        const uint16_t indices16[] = { 0, 1, 2 };
        const uint32_t indices32[] = { 0, 1, 2 };
        const void*    pIndices = indexType == INDEX_TYPE_UINT16 ? (const void*)indices16 : indices32;
        const uint32_t indexBytes = indexType == INDEX_TYPE_UINT16 ? sizeof(indices16) : sizeof(indices32);
        const float    positions[] = { -1, 2, 3, 4, -5, 6, 0, 1, -2 };
        const uint32_t geometrySize = sizeof(geometry) + sizeof(Draw);
        const uint32_t dataSize = sizeof(data);
        const uint32_t shadowSize = sizeof(shadow) + indexBytes + sizeof(positions) + (rendererLayout ? sizeof(normals) + sizeof(uvs) : 0);
        FileStream     stream = {};
        ASSERT_TRUE(OpenStreamFromPath(DirectoryOtherFiles, "scene.bin", FileModeWrite, &stream));
        WriteToStream(&stream, GEOMETRY_FILE_MAGIC_STR, sizeof(GEOMETRY_FILE_MAGIC_STR));
        WriteToStream(&stream, &GEOMETRY_FILE_VERSION, sizeof(GEOMETRY_FILE_VERSION));
        WriteToStream(&stream, GEOMETRY_FILE_LAYOUT, sizeof(GEOMETRY_FILE_LAYOUT));
        WriteToStream(&stream, &geometrySize, sizeof(geometrySize));
        WriteToStream(&stream, &geometry, sizeof(geometry));
        WriteToStream(&stream, &Draw, sizeof(Draw));
        WriteToStream(&stream, &dataSize, sizeof(dataSize));
        WriteToStream(&stream, &data, sizeof(data));
        WriteToStream(&stream, &shadowSize, sizeof(shadowSize));
        WriteToStream(&stream, &shadow, sizeof(shadow));
        WriteToStream(&stream, pIndices, indexBytes);
        WriteToStream(&stream, positions, sizeof(positions));
        if (rendererLayout)
        {
            WriteToStream(&stream, normals, sizeof(normals));
            WriteToStream(&stream, uvs, sizeof(uvs));
        }
        CloseStream(&stream);
    }
};
