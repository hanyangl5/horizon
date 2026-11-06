#include <gtest/gtest.h>
#include <filesystem>
#include <cmath>
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
#include "../../Examples/Renderer/Source/DeferredLighting.h"
#include "../../Examples/Renderer/Source/GpuDrawList.h"
#include "Core/IUniquePtr.h"
#include "Resources/IResourceLoader.h"
#include "Graphics/RenderContext.h"
#include "Core/IThread.h"
#include "Core/ITime.h"
#include "Core/IContainer.h"
#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"
#include <d3d12sdklayers.h>

static hz::AssetID assetID(uint32_t value) { return { .high = 1, .low = value }; }

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

    hz::AssetLoader getLoader()
    {
        return { .pUserData = this, .pBegin = begin, .pPoll = poll, .pDestroyRequest = destroyRequest, .pDestroyPayload = destroyPayload };
    }

    hz::AssetLoader getGpuLoader()
    {
        hz::AssetLoader loader = getLoader();
        loader.pUpload = upload;
        loader.pPollUpload = pollUpload;
        loader.pWaitUpload = waitUpload;
        return loader;
    }
    static void upload(void*, const hz::AssetManager& assetManager, hz::Span<hz::AssetHandle> dependencies, void* pUser)
    {
        ++((TestAssetLoader*)pUser)->uploads;
        for (hz::AssetHandle dependency : dependencies)
            EXPECT_EQ(assetManager.getState(dependency), hz::AssetState::Ready);
    }
    static hz::AssetLoadStatus pollUpload(void*, void* pUser)
    {
        TestAssetLoader& loader = *(TestAssetLoader*)pUser;
        if (!loader.uploadReady)
            return hz::AssetLoadStatus::Pending;
        return loader.uploadFailed ? hz::AssetLoadStatus::Failed : hz::AssetLoadStatus::Succeeded;
    }
    static void waitUpload(void*, void* pUser) { ++((TestAssetLoader*)pUser)->uploadWaits; }

    static void* begin(const hz::AssetCatalogEntry& entry, hz::AssetHandle, void* pUser)
    {
        TestAssetLoader& loader = *(TestAssetLoader*)pUser;
        const uint32_t   id = (uint32_t)entry.id.low;
        ++loader.begun;
        if (loader.failBegin == id)
            return nullptr;
        Payload* pPayload = tf_new(Payload);
        pPayload->id = id;
        Job* pJob = tf_new(Job);
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

    static void destroyRequest(void* pRequest, void* pUser)
    {
        TestAssetLoader& loader = *(TestAssetLoader*)pUser;
        Job*             pJob = (Job*)pRequest;
        ++loader.requestsDestroyed;
        if (pJob->pPayload)
            destroyPayload(pJob->pPayload, pUser);
        tf_delete(pJob);
    }

    static void destroyPayload(void* pPayload, void* pUser)
    {
        TestAssetLoader& loader = *(TestAssetLoader*)pUser;
        Payload*         pValue = (Payload*)pPayload;
        loader.destroyed.push_back(pValue->id);
        tf_delete(pValue);
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
        ASSERT_TRUE(initMemAlloc(nullptr));
        initialStats = memGetTrackingStats();
    }
    void TearDown() override
    {
        const MemoryTrackingStats stats = memGetTrackingStats();
        if (stats.trackingEnabled)
        {
            EXPECT_EQ(stats.liveAllocationCount, initialStats.liveAllocationCount);
            EXPECT_EQ(stats.liveRequestedBytes, initialStats.liveRequestedBytes);
        }
        exitMemAlloc();
    }
    static void add(hz::AssetCatalog& catalog, uint32_t id, hz::Span<hz::AssetID> dependencies = {})
    {
        ASSERT_TRUE(catalog.add({ .id = assetID(id),
                                  .pSourcePath = "test.gltf",
                                  .location = { .pPath = "test.sceneasset.json" },
                                  .dependencies = dependencies }));
    }
    MemoryTrackingStats initialStats = {};
};

TEST_F(AssetManagerTest, DeduplicatesRequestsAndRejectsStaleHandlesAfterReuse)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    TestAssetLoader  loader;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle first = assetManager.request(assetID(1));
    ASSERT_TRUE(first.isValid());
    EXPECT_EQ(assetManager.request(assetID(1)), first);
    EXPECT_EQ(assetManager.getState(first), hz::AssetState::Queued);
    assetManager.update();
    EXPECT_EQ(assetManager.getState(first), hz::AssetState::Ready);
    EXPECT_EQ(loader.begun, 1u);
    EXPECT_TRUE(assetManager.release(first));
    assetManager.update();
    EXPECT_EQ(assetManager.getState(first), hz::AssetState::Ready);
    EXPECT_TRUE(assetManager.release(first));
    assetManager.update();
    EXPECT_EQ(assetManager.getCount(), 0u);
    const hz::AssetHandle second = assetManager.request(assetID(1));
    EXPECT_EQ(second.index, first.index);
    EXPECT_NE(second.generation, first.generation);
    EXPECT_FALSE(assetManager.release(first));
    EXPECT_EQ(assetManager.getState(first), hz::AssetState::Invalid);
    assetManager.update();
    assetManager.shutdown();
    EXPECT_EQ(loader.destroyed.size(), 2u);
}

TEST_F(AssetManagerTest, SharesDependenciesAndDestroysParentsBeforeChildren)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1), assetID(1) });
    add(catalog, 3, { assetID(1) });
    EXPECT_EQ(catalog.find(assetID(2))->dependencies.count, 1u);
    TestAssetLoader  loader;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle a = assetManager.request(assetID(2));
    const hz::AssetHandle b = assetManager.request(assetID(3));
    assetManager.update();
    EXPECT_EQ(loader.begun, 3u);
    EXPECT_TRUE(assetManager.release(a));
    assetManager.update();
    EXPECT_EQ(assetManager.getCount(), 2u);
    EXPECT_EQ(assetManager.getState(b), hz::AssetState::Ready);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2 }));
    assetManager.shutdown();
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2, 3, 1 }));
}

TEST_F(AssetManagerTest, RejectsInvalidDependencyGraphsWithoutChangingExistingReferences)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1), assetID(99) });
    add(catalog, 3, { assetID(4) });
    add(catalog, 4, { assetID(3) });
    ASSERT_TRUE(
        catalog.add({ .id = assetID(5), .type = hz::AssetType::Texture, .pSourcePath = "test.png", .location = { .pPath = "test.dds" } }));
    TestAssetLoader  loader;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle existing = assetManager.request(assetID(1));
    hz::AssetError        error = hz::AssetError::None;
    EXPECT_FALSE(assetManager.request(assetID(2), &error).isValid());
    EXPECT_EQ(error, hz::AssetError::UnknownAsset);
    EXPECT_FALSE(assetManager.request(assetID(3), &error).isValid());
    EXPECT_EQ(error, hz::AssetError::DependencyCycle);
    EXPECT_FALSE(assetManager.request(assetID(5), &error).isValid());
    EXPECT_EQ(error, hz::AssetError::MissingLoader);
    EXPECT_EQ(assetManager.getCount(), 1u);
    EXPECT_TRUE(assetManager.release(existing));
    assetManager.update();
    EXPECT_EQ(assetManager.getCount(), 0u);
    EXPECT_EQ(loader.begun, 0u);
}

TEST_F(AssetManagerTest, CancellationDrainsLateResultsWithoutPublishingOrReusingTheSlot)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle handle = assetManager.request(assetID(1));
    assetManager.update();
    ASSERT_EQ(assetManager.getState(handle), hz::AssetState::LoadingCPU);
    EXPECT_TRUE(assetManager.release(handle));
    assetManager.update();
    EXPECT_EQ(assetManager.getState(handle), hz::AssetState::Retiring);
    EXPECT_EQ(assetManager.getScene(handle), nullptr);
    hz::AssetError error;
    EXPECT_FALSE(assetManager.request(assetID(1), &error).isValid());
    EXPECT_EQ(error, hz::AssetError::Retiring);
    EXPECT_EQ(loader.requestsDestroyed, 0u);
    loader.jobs[1]->ready = true;
    assetManager.update();
    EXPECT_EQ(assetManager.getState(handle), hz::AssetState::Invalid);
    EXPECT_EQ(loader.requestsDestroyed, 1u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1 }));
}

TEST_F(AssetManagerTest, WaitsForDependenciesBeforePublishingDecodedPayload)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1) });
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle handle = assetManager.request(assetID(2));
    assetManager.update();
    loader.jobs[2]->ready = true;
    assetManager.update();
    EXPECT_EQ(assetManager.getState(handle), hz::AssetState::WaitingDependencies);
    EXPECT_EQ(assetManager.getScene(handle), nullptr);
    loader.jobs[1]->ready = true;
    assetManager.update();
    EXPECT_EQ(assetManager.getState(handle), hz::AssetState::Ready);
    assetManager.shutdown();
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2, 1 }));
}

TEST_F(AssetManagerTest, DependencyFailureDrainsPendingParentAndReleasesPartialPayloads)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1) });
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle parent = assetManager.request(assetID(2));
    assetManager.update();
    loader.jobs[1]->failed = true;
    loader.jobs[1]->ready = true;
    assetManager.update();
    EXPECT_EQ(assetManager.getState(parent), hz::AssetState::Failed);
    EXPECT_EQ(assetManager.getError(parent), hz::AssetError::DependencyFailed);
    EXPECT_EQ(assetManager.getScene(parent), nullptr);
    EXPECT_EQ(assetManager.getCount(), 2u);
    loader.jobs[2]->ready = true;
    assetManager.update();
    EXPECT_EQ(assetManager.getCount(), 1u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1, 2 }));
    EXPECT_TRUE(assetManager.release(parent));
    assetManager.update();
    EXPECT_EQ(assetManager.getCount(), 0u);
    loader.failBegin = 1;
    const hz::AssetHandle failed = assetManager.request(assetID(1));
    assetManager.update();
    EXPECT_EQ(assetManager.getState(failed), hz::AssetState::Failed);
    EXPECT_EQ(assetManager.getError(failed), hz::AssetError::LoadFailed);
}

TEST_F(AssetManagerTest, ShutdownDrainsPendingJobsAndIsIdempotent)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1) });
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Scene, loader.getLoader());
    assetManager.request(assetID(2));
    assetManager.update();
    assetManager.shutdown();
    EXPECT_EQ(loader.requestsDestroyed, 2u);
    EXPECT_EQ(loader.destroyed.size(), 2u);
    EXPECT_EQ(assetManager.getCount(), 0u);
    hz::AssetError error;
    EXPECT_FALSE(assetManager.request(assetID(1), &error).isValid());
    EXPECT_EQ(error, hz::AssetError::ShuttingDown);
    assetManager.shutdown();
    EXPECT_EQ(loader.requestsDestroyed, 2u);
}

TEST_F(AssetManagerTest, TraversesAndRetiresDeepDependenciesIteratively)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    for (uint32_t i = 2; i <= 1024; ++i)
        add(catalog, i, { assetID(i - 1) });
    TestAssetLoader  loader;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle handle = assetManager.request(assetID(1024));
    assetManager.update();
    EXPECT_EQ(assetManager.getState(handle), hz::AssetState::Ready);
    EXPECT_TRUE(assetManager.release(handle));
    assetManager.update();
    EXPECT_EQ(assetManager.getCount(), 0u);
    ASSERT_EQ(loader.destroyed.size(), 1024u);
    EXPECT_EQ(loader.destroyed.front(), 1024u);
    EXPECT_EQ(loader.destroyed.back(), 1u);
}

TEST_F(AssetManagerTest, UploadWaitsForDependenciesAndCancellationDrainsBeforeDestruction)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1) });
    TestAssetLoader  loader;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Scene, loader.getGpuLoader());
    const hz::AssetHandle parent = assetManager.request(assetID(2));
    assetManager.update();
    EXPECT_EQ(loader.uploads, 1u);
    EXPECT_EQ(assetManager.getState(parent), hz::AssetState::WaitingDependencies);
    const hz::AssetHandle child = assetManager.find(assetID(1));
    EXPECT_EQ(assetManager.getState(child), hz::AssetState::UploadingGPU);
    assetManager.release(parent);
    assetManager.update();
    EXPECT_EQ(assetManager.getState(parent), hz::AssetState::Invalid);
    EXPECT_EQ(assetManager.getState(child), hz::AssetState::Retiring);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2 }));
    loader.uploadReady = true;
    assetManager.update();
    EXPECT_EQ(assetManager.getCount(), 0u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2, 1 }));
    EXPECT_EQ(loader.uploadWaits, 0u);
}

TEST_F(AssetManagerTest, UploadFailureAndShutdownUseTheSamePayloadCleanup)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    TestAssetLoader  loader;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Scene, loader.getGpuLoader());
    const hz::AssetHandle handle = assetManager.request(assetID(1));
    assetManager.update();
    loader.uploadReady = true;
    loader.uploadFailed = true;
    assetManager.update();
    EXPECT_EQ(assetManager.getState(handle), hz::AssetState::Failed);
    EXPECT_EQ(assetManager.getError(handle), hz::AssetError::UploadFailed);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1 }));
    assetManager.release(handle);
    assetManager.update();
    loader.uploadReady = false;
    assetManager.request(assetID(1));
    assetManager.update();
    assetManager.shutdown();
    EXPECT_EQ(loader.uploadWaits, 1u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1, 1 }));
}

TEST_F(AssetManagerTest, GpuTablesRebuildOnlyWhenTheReadySetChanges)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2);
    TestAssetLoader  loader;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle first = assetManager.request(assetID(1));
    assetManager.update();
    const uint64_t            revision = assetManager.getGpuTables().revision;
    const MemoryTrackingStats before = memGetTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
    {
        EXPECT_EQ(assetManager.getGpuTables().revision, revision);
        assetManager.update();
    }
    const MemoryTrackingStats after = memGetTrackingStats();
    if (after.trackingEnabled)
    {
        EXPECT_EQ(after.totalAllocationCount, before.totalAllocationCount);
        EXPECT_EQ(after.reallocationCount, before.reallocationCount);
    }
    assetManager.release(first);
    const uint64_t released = assetManager.getGpuTables().revision;
    EXPECT_NE(released, revision);
    // Without a render context nothing can still be using the payload.
    assetManager.update();
    EXPECT_EQ(assetManager.getState(first), hz::AssetState::Invalid);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1 }));
    const hz::AssetHandle second = assetManager.request(assetID(2));
    EXPECT_EQ(assetManager.getGpuTables().revision, released);
    assetManager.update();
    EXPECT_EQ(assetManager.getState(second), hz::AssetState::Ready);
    EXPECT_NE(assetManager.getGpuTables().revision, released);
    assetManager.shutdown();
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1, 2 }));
}

static const char kEmptyScene[] = R"json({
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
        ASSERT_TRUE(initFileSystem(&desc));
        char id[hz::kIDStringCapacity];
        hz::AssetID::create().toString(id);
        directory = std::filesystem::temp_directory_path() / id;
        std::filesystem::create_directory(directory);
        fileSystem = *pSystemFileIO;
        fileSystem.GetResourceMount = nullptr;
        fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, directory.string().c_str());
        FileStream stream = {};
        ASSERT_TRUE(fsOpenStreamFromPath(RD_OTHER_FILES, "scene.sceneasset.json", FM_WRITE, &stream));
        EXPECT_EQ(fsWriteToStream(&stream, kEmptyScene, sizeof(kEmptyScene) - 1), sizeof(kEmptyScene) - 1);
        fsCloseStream(&stream);
    }
    void TearDown() override
    {
        exitFileSystem();
        std::filesystem::remove_all(directory);
        AssetManagerTest::TearDown();
    }
    static void wait(hz::AssetManager& assetManager, hz::AssetHandle handle)
    {
        for (uint32_t i = 0; i < 5000; ++i)
        {
            assetManager.update();
            if (assetManager.getState(handle) == hz::AssetState::Ready || assetManager.getState(handle) == hz::AssetState::Failed)
                return;
            threadSleep(1);
        }
        FAIL() << "SceneAsset load timed out";
    }
    std::filesystem::path directory;
    IFileSystem           fileSystem = {};
};

TEST_F(SceneAssetLoaderTest, DecodesSceneAsynchronouslyAndPublishesTypedPayload)
{
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.parse(kEmptyScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(scene, "scene.sceneasset.json"));
    hz::Array<char> index;
    ASSERT_TRUE(catalog.serialize(index));
    FileStream stream = {};
    ASSERT_TRUE(fsOpenStreamFromPath(RD_OTHER_FILES, "scene.assetcatalog.json", FM_WRITE, &stream));
    EXPECT_EQ(fsWriteToStream(&stream, index.data(), index.size() - 1), index.size() - 1);
    fsCloseStream(&stream);
    catalog.clear();
    ASSERT_TRUE(catalog.load(RD_OTHER_FILES, "scene.assetcatalog.json"));
    hz::SceneAssetLoader loader(RD_OTHER_FILES);
    hz::AssetManager     assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle handle = assetManager.request(scene.getID());
    EXPECT_EQ(assetManager.getScene(handle), nullptr);
    wait(assetManager, handle);
    ASSERT_EQ(assetManager.getState(handle), hz::AssetState::Ready);
    ASSERT_NE(assetManager.getScene(handle), nullptr);
    EXPECT_EQ(assetManager.getScene(handle)->getID(), scene.getID());
    assetManager.release(handle);
    assetManager.update();
    EXPECT_EQ(assetManager.getScene(handle), nullptr);
}

TEST_F(SceneAssetLoaderTest, RejectsMissingFileAndMismatchedIdentityAndDrainsOnShutdown)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.add({ .id = assetID(1), .pSourcePath = "scene.gltf", .location = { .pPath = "missing.sceneasset.json" } }));
    ASSERT_TRUE(catalog.add({ .id = assetID(2), .pSourcePath = "scene.gltf", .location = { .pPath = "scene.sceneasset.json" } }));
    hz::SceneAssetLoader loader(RD_OTHER_FILES);
    hz::AssetManager     assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle missing = assetManager.request(assetID(1));
    const hz::AssetHandle mismatched = assetManager.request(assetID(2));
    wait(assetManager, missing);
    wait(assetManager, mismatched);
    EXPECT_EQ(assetManager.getError(missing), hz::AssetError::LoadFailed);
    EXPECT_EQ(assetManager.getError(mismatched), hz::AssetError::LoadFailed);
    assetManager.release(mismatched);
    assetManager.update();
    assetManager.request(assetID(2));
    assetManager.update();
    assetManager.shutdown();
    EXPECT_EQ(assetManager.getCount(), 0u);
}

TEST_F(SceneAssetLoaderTest, RejectsPayloadWithDifferentDependenciesFromCatalog)
{
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.parse(kEmptyScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.add({ .id = scene.getID(),
                              .pSourcePath = "scene.gltf",
                              .location = { .pPath = "scene.sceneasset.json" },
                              .dependencies = { assetID(1) } }));
    ASSERT_TRUE(catalog.add({ .id = assetID(1),
                              .type = hz::AssetType::Mesh,
                              .pSourcePath = "scene.gltf",
                              .location = { .pPath = "scene.sceneasset.json", .elementIndex = 0 } }));
    hz::SceneAssetLoader loader(RD_OTHER_FILES);
    TestAssetLoader      meshLoader;
    hz::AssetManager     assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Scene, loader.getLoader());
    assetManager.setLoader(hz::AssetType::Mesh, meshLoader.getLoader());
    const hz::AssetHandle handle = assetManager.request(scene.getID());
    wait(assetManager, handle);
    EXPECT_EQ(assetManager.getState(handle), hz::AssetState::Failed);
    EXPECT_EQ(assetManager.getScene(handle), nullptr);
    EXPECT_EQ(meshLoader.destroyed, (std::vector<uint32_t>{ 1 }));
}

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
        ASSERT_TRUE(fsOpenStreamFromPath(RD_OTHER_FILES, "pixel.dds", FM_WRITE, &stream));
        EXPECT_EQ(fsWriteToStream(&stream, dds, sizeof(dds)), sizeof(dds));
        fsCloseStream(&stream);
    }
};

TEST_F(TextureAssetLiveTest, UploadsSamplesAndRetiresTextureAfterSubmission)
{
    writeTexture();
    hz::RenderContext context({ .pAppName = "TextureAssetTest", .enableGpuValidation = true });
    hz::AssetCatalog  catalog;
    ASSERT_TRUE(catalog.add(
        { .id = assetID(1), .type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "pixel.dds" } }));
    hz::TextureAssetLoader loader(context, RD_OTHER_FILES);
    hz::AssetManager       assetManager(catalog, &context);
    assetManager.setLoader(hz::AssetType::Texture, loader.getLoader());
    const hz::AssetHandle handle = assetManager.request(assetID(1));
    EXPECT_EQ(assetManager.request(assetID(1)), handle);
    assetManager.release(handle);
    wait(assetManager, handle);
    ASSERT_EQ(assetManager.getState(handle), hz::AssetState::Ready);
    const hz::GPUTexture& texture = assetManager.getTexture(handle)->getTexture();
    ASSERT_TRUE(texture.isValid());
    const char      shader[] = R"hlsl(
        cbuffer RootConstant0 { uint textureIndex; uint outputIndex; };
        [numthreads(1,1,1)] void CSMain() {
            Texture2D<float4> source = ResourceDescriptorHeap[textureIndex];
            RWStructuredBuffer<uint4> output = ResourceDescriptorHeap[outputIndex];
            output[0] = uint4(round(source.Load(int3(0,0,0)) * 255.0));
        }
    )hlsl";
    hz::GPUPipeline pipeline = context.createComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP,
                                                                                             .pSource = shader,
                                                                                             .sourceSize = sizeof(shader) - 1,
                                                                                             .pEntryPoint = "CSMain" } } },
                                                               .pName = "TextureAsset.Readback" });
    ASSERT_TRUE(pipeline.isValid());
    hz::GPUBuffer    output = context.createBuffer({ .size = 16,
                                                     .elementCount = 1,
                                                     .structStride = 16,
                                                     .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                     .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                     .descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer    readback = context.createBuffer({ .size = 16,
                                                       .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                       .startState = RESOURCE_STATE_COPY_DEST,
                                                       .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::CommandList& commands = context.acquireCommandList();
    commands.setPipeline(pipeline);
    commands.setRootConstant({ texture.getSrvIndex(), output.getUavIndex() });
    commands.dispatch(1, 1, 1, { .sampledTextures = { &texture }, .storageBuffers = { &output } });
    commands.copyBuffer(readback, 0, output, 0, 16);
    // The open command list references the texture, so its release waits for that work.
    assetManager.release(handle);
    assetManager.update();
    EXPECT_EQ(assetManager.getState(handle), hz::AssetState::Retiring);
    EXPECT_EQ(assetManager.getCount(), 1u);
    const hz::SubmitHandle submission = context.submit(commands);
    assetManager.shutdown();
    EXPECT_TRUE(context.isComplete(submission));
    EXPECT_EQ(assetManager.getCount(), 0u);
    const uint32_t* pPixel = (const uint32_t*)readback.get()->pCpuMappedAddress;
    ASSERT_NE(pPixel, nullptr);
    EXPECT_EQ(pPixel[0], 255u);
    EXPECT_EQ(pPixel[1], 128u);
    EXPECT_EQ(pPixel[2], 64u);
    EXPECT_EQ(pPixel[3], 255u);
}

TEST_F(SceneAssetLoaderTest, RendererMotionUsesPerspectiveCorrectSubmittedTransforms)
{
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, shaders.string().c_str());
    hz::RenderContext context({ .pAppName = "RendererMotionTest", .enableGpuValidation = true });
    hz::GPUPipeline   pipeline = context.createGraphicsPipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                    { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                        .sourceDirectory = RD_OTHER_FILES,
                        .pFileName = "Geometry.slang" },
        .colorTargets = { { .format = hz::Format::R8G8B8A8_UNORM },
                          { .format = hz::Format::R8G8B8A8_UNORM },
                          { .format = hz::Format::R8G8B8A8_UNORM },
                          { .format = hz::Format::R8G8B8A8_UNORM } },
    });
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, directory.string().c_str());
    ASSERT_TRUE(pipeline.isValid());
    struct FrameData
    {
        Matrix4 viewProjection;
        Matrix4 previousViewProjection;
        Matrix4 inverseViewProjection;
        Vector4 eye;
    };
    // Project x/z, y/z at constant depth, with vertex w = 1, 1, 2.
    const Matrix4      projection(Vector4(1, 0, 0, 0), Vector4(0, 1, 0, 0), Vector4(0, 0, 0.5f, 1), Vector4(0));
    const FrameData    frameData = { .viewProjection = projection,
                                     .previousViewProjection = projection,
                                     .inverseViewProjection = Matrix4::identity(),
                                     .eye = Vector4(0, 0, 0, 1) };
    const float        positions[] = { -0.8f, -0.8f, 1, 0.6f, -0.8f, 1, -0.2f, 1, 2 };
    const uint32_t     normals[] = { 0x80008000, 0x80008000, 0x80008000 }, texcoords[] = { 0, 0, 0 };
    hz::RenderInstance instance;
    instance.current = Matrix4::translation(Vector3(0.2f, 0, 0));
    const hz::GpuMaterialRecord material;
    const auto                  input = [&](const void* data, uint32_t size, uint32_t stride = 0)
    {
        return context.createBuffer({ .size = size,
                                      .elementCount = stride ? size / stride : size / 4,
                                      .structStride = stride,
                                      .pInitialData = data,
                                      .initialDataSize = size,
                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                      .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                      .descriptors = stride ? DESCRIPTOR_TYPE_BUFFER : DESCRIPTOR_TYPE_BUFFER_RAW });
    };
    hz::GPUBuffer  frame = input(&frameData, sizeof(frameData), sizeof(frameData));
    hz::GPUBuffer  instances = input(&instance, sizeof(instance)), materials = input(&material, sizeof(material));
    hz::GPUBuffer  vertices = input(positions, sizeof(positions)), normal = input(normals, sizeof(normals));
    hz::GPUBuffer  uv = input(texcoords, sizeof(texcoords));
    hz::GPUSampler sampler = context.createSampler();
    hz::GPUTexture targets[4];
    for (hz::GPUTexture& target : targets)
        target = context.createTexture({ .width = 64,
                                         .height = 64,
                                         .format = hz::Format::R8G8B8A8_UNORM,
                                         .startState = RESOURCE_STATE_RENDER_TARGET,
                                         .descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                         .renderTarget = true });
    const char      readPixel[] = R"hlsl(
        cbuffer RootConstant0 { uint textureIndex; uint outputIndex; };
        [numthreads(1,1,1)] void CSMain() {
            Texture2D<float4> motion = ResourceDescriptorHeap[textureIndex];
            RWStructuredBuffer<uint4> output = ResourceDescriptorHeap[outputIndex];
            output[0] = uint4(round(motion.Load(int3(32,32,0)) * 255.0));
        }
    )hlsl";
    hz::GPUPipeline reader = context.createComputePipeline(
        { .shaderDesc = {
              .stages = {
                  { .stage = SHADER_STAGE_COMP, .pSource = readPixel, .sourceSize = sizeof(readPixel) - 1, .pEntryPoint = "CSMain" } } } });
    hz::GPUBuffer output = context.createBuffer({ .size = 16,
                                                  .elementCount = 1,
                                                  .structStride = 16,
                                                  .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                  .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                  .descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer readback = context.createBuffer({ .size = 16,
                                                    .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                    .startState = RESOURCE_STATE_COPY_DEST,
                                                    .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    ASSERT_TRUE(reader.isValid() && sampler.isValid());
    hz::CommandList& commands = context.acquireCommandList();
    commands.beginRendering({ .colorAttachments = {
        { .pTexture = &targets[0], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
        { .pTexture = &targets[1], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
        { .pTexture = &targets[2], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
        { .pTexture = &targets[3], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
    } }, { .buffers = { &frame, &instances, &materials, &vertices, &normal, &uv } });
    commands.setViewport(0, 0, 64, 64);
    commands.setScissor(0, 0, 64, 64);
    commands.setPipeline(pipeline);
    commands.setRootConstant({ 0, 0, 7, frame.getSrvIndex(), instances.getSrvIndex(), materials.getSrvIndex(), UINT32_MAX,
                               sampler.getIndex(), vertices.getSrvIndex(), normal.getSrvIndex(), uv.getSrvIndex() });
    commands.draw(3);
    commands.endRendering();
    commands.setPipeline(reader);
    commands.setRootConstant({ targets[3].getSrvIndex(), output.getUavIndex() });
    commands.dispatch(1, 1, 1, { .sampledTextures = { &targets[3] }, .storageBuffers = { &output } });
    commands.copyBuffer(readback, 0, output, 0, 16);
    context.wait(context.submit(commands));
    const uint32_t* pixel = (const uint32_t*)readback.get()->pCpuMappedAddress;
    ASSERT_EQ(pixel[3], 7u); // The sampled pixel was covered by the triangle.
    const uint32_t packed = pixel[0] | (pixel[1] << 8) | (pixel[2] << 16);
    const float    motion = (float)(packed & 0xfff) / 4095.0f * 2 - 1;
    const float    topWeight = ((1 - 2 * 32.5f / 64) + 0.8f) / 1.3f;
    const float    expected = 0.2f * (1 - topWeight * 0.5f) * 0.5f;
    EXPECT_NEAR(motion, expected, 1.0f / 4095.0f + 1e-5f);
    EXPECT_NEAR((float)(packed >> 12) / 4095.0f * 2 - 1, 0, 1.0f / 4095.0f + 1e-5f);
}

TEST_F(SceneAssetLoaderTest, RendererGpuCullingSeparatesDrawsAndKeepsShadowCasters)
{
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, shaders.string().c_str());
    hz::RenderContext context({ .pAppName = "RendererCullingTest", .enableGpuValidation = true });
    hz::GPUPipeline   pipeline =
        context.createComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSMain" } },
                                                        .sourceDirectory = RD_OTHER_FILES,
                                                        .pFileName = "FrustumCulling.slang" } });
    hz::GPUPipeline clear =
        context.createComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "ClearStatistics" } },
                                                        .sourceDirectory = RD_OTHER_FILES,
                                                        .pFileName = "FrustumCulling.slang" } });
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, directory.string().c_str());
    ASSERT_TRUE(pipeline.isValid() && clear.isValid());
    struct FrameData
    {
        Matrix4 current;
        Matrix4 previous;
        Matrix4 inverse;
        Vector4 eye;
    };
    FrameData          camera = { Matrix4::identity(), Matrix4::identity(), Matrix4::identity(), Vector4(0, 0, 0, 1) };
    hz::RenderInstance instances[5];
    const float        bounds[5][6] = { { -.2f, -.2f, .2f, .2f, .2f, .8f },
                                        { 1.1f, 0, .2f, 1.5f, .2f, .8f },
                                        { 0, 0, -.8f, .2f, .2f, -.2f },
                                        { -.2f, -.2f, .2f, .2f, .2f, .8f },
                                        { -2, -2, -1, 2, 2, 2 } };
    uint32_t           records[5][12] = {};
    for (uint32_t i = 0; i < 5; ++i)
    {
        memcpy(instances[i].bounds.minimum, bounds[i], 12);
        memcpy(instances[i].bounds.maximum, bounds[i] + 3, 12);
        records[i][0] = i;
        records[i][3] = i == 3 ? 2u : 0u;
        records[i][11] = 3 * (i + 1);
    }
    const auto input = [&](const void* data, uint32_t size, uint32_t stride = 0)
    {
        return context.createBuffer({ .size = size,
                                      .elementCount = stride ? size / stride : size / 4,
                                      .structStride = stride,
                                      .pInitialData = data,
                                      .initialDataSize = size,
                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                      .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                      .descriptors = stride ? DESCRIPTOR_TYPE_BUFFER : DESCRIPTOR_TYPE_BUFFER_RAW,
                                      .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    };
    hz::GPUBuffer frame = input(&camera, sizeof(camera), sizeof(camera));
    hz::GPUBuffer instanceBuffer = input(instances, sizeof(instances));
    hz::GPUBuffer drawBuffer = input(records, sizeof(records));
    hz::GPUBuffer outputs[3];
    for (hz::GPUBuffer& buffer : outputs)
    {
        buffer = context.createBuffer({ .size = 100,
                                        .elementCount = 25,
                                        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                        .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                        .descriptors = DESCRIPTOR_TYPE_RW_BUFFER_RAW });
    }
    hz::GPUBuffer  statistics = context.createBuffer({ .size = 16,
                                                       .elementCount = 4,
                                                       .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                       .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                       .descriptors = DESCRIPTOR_TYPE_RW_BUFFER_RAW });
    hz::GPUBuffer  readback = context.createBuffer({ .size = 316,
                                                     .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                     .startState = RESOURCE_STATE_COPY_DEST,
                                                     .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    // With a 90-degree frustum, the first camera sees centers (3,0,-2) and (4,0,-2), while (12,0,-2) is outside right.
    // Moving to (9,0,2) and looking diagonally toward +X/-Z makes that outside box visible and leaves both old boxes behind.
    const float    perspectiveBounds[5][6] = { { 2.75f, -.25f, -2.25f, 3.25f, .25f, -1.75f },
                                               { 11.75f, -.25f, -2.25f, 12.25f, .25f, -1.75f },
                                               { 2.75f, -.25f, 3.75f, 3.25f, .25f, 4.25f },
                                               { 3.75f, -.25f, -2.25f, 4.25f, .25f, -1.75f },
                                               { -20, -20, -20, 30, 20, 20 } };
    const uint32_t visibleMasks[] = { 0x19, 0x19, 0x12 }, triangleCounts[] = { 10, 10, 7 };
    for (uint32_t cameraCase = 0; cameraCase < 3; ++cameraCase)
    {
        SCOPED_TRACE(cameraCase);
        if (cameraCase)
        {
            for (uint32_t i = 0; i < 5; ++i)
            {
                memcpy(instances[i].bounds.minimum, perspectiveBounds[i], 12);
                memcpy(instances[i].bounds.maximum, perspectiveBounds[i] + 3, 12);
            }
            const Matrix4 pose = cameraCase == 1 ? Matrix4::translation(Vector3(3, 0, 2))
                                                 : Matrix4::translation(Vector3(9, 0, 2)) * Matrix4::rotationY(-PI / 4.0f);
            camera.current = Matrix4::perspectiveRH(PI / 2.0f, 1.0f, 0.1f, 10.0f) * inverse(pose);
            camera.previous = camera.current;
            camera.inverse = inverse(camera.current);
            camera.eye = pose.getCol3();
            ASSERT_TRUE(context.updateBuffer(frame, 0, &camera, sizeof(camera)));
            ASSERT_TRUE(context.updateBuffer(instanceBuffer, 0, instances, sizeof(instances)));
        }
        for (bool culling : { true, false })
        {
            SCOPED_TRACE(culling);
            hz::CommandList& commands = context.acquireCommandList();
            commands.setPipeline(clear);
            commands.setRootConstant({ statistics.getUavIndex() }, 5);
            commands.dispatch(1, 1, 1, { .storageBuffers = { &statistics } });
            commands.setPipeline(pipeline);
            commands.setRootConstant({ frame.getSrvIndex(), drawBuffer.getSrvIndex(), instanceBuffer.getSrvIndex(),
                                       outputs[0].getUavIndex(), outputs[1].getUavIndex(), statistics.getUavIndex(), 5, culling ? 1u : 0u,
                                       outputs[2].getUavIndex() });
            commands.dispatch(1, 1, 1,
                              { .buffers = { &frame, &drawBuffer, &instanceBuffer },
                                .storageBuffers = { &outputs[0], &outputs[1], &outputs[2], &statistics } });
            for (uint32_t i = 0; i < 3; ++i)
            {
                commands.copyBuffer(readback, i * 100, outputs[i], 0, 100);
            }
            commands.copyBuffer(readback, 300, statistics, 0, 16);
            context.wait(context.submit(commands));
            const uint32_t* data = (const uint32_t*)readback.get()->pCpuMappedAddress;
            ASSERT_NE(data, nullptr);
            for (uint32_t i = 0; i < 5; ++i)
            {
                const bool visible = !culling || (visibleMasks[cameraCase] & (1u << i));
                for (uint32_t stream = 0; stream < 3; ++stream)
                {
                    const uint32_t* args = data + stream * 25 + i * 5;
                    EXPECT_EQ(args[0], i);
                    EXPECT_EQ(args[1], 3 * (i + 1));
                    EXPECT_EQ(args[2], stream == 2 ? uint32_t(i != 3) : uint32_t(visible && (stream == 1 ? i == 3 : i != 3)));
                    EXPECT_EQ(args[3], 0u);
                    EXPECT_EQ(args[4], 0u);
                }
            }
            EXPECT_EQ(data[75], culling ? (cameraCase == 2 ? 2u : 3u) : 5u);
            EXPECT_EQ(data[76], culling ? triangleCounts[cameraCase] : 15u);
        }
    }
}

TEST_F(TextureAssetLiveTest, CancelsUploadsAndReportsInvalidFiles)
{
    writeTexture();
    std::filesystem::copy_file(directory / "pixel.dds", directory / "truncated.dds");
    std::filesystem::resize_file(directory / "truncated.dds", 128);
    FileStream broken = {};
    ASSERT_TRUE(fsOpenStreamFromPath(RD_OTHER_FILES, "broken.dds", FM_WRITE, &broken));
    const uint32_t invalidHeader = 0;
    fsWriteToStream(&broken, &invalidHeader, sizeof(invalidHeader));
    fsCloseStream(&broken);
    hz::RenderContext context({ .pAppName = "TextureAssetCancelTest" });
    hz::AssetCatalog  catalog;
    ASSERT_TRUE(catalog.add(
        { .id = assetID(1), .type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "pixel.dds" } }));
    ASSERT_TRUE(catalog.add(
        { .id = assetID(2), .type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "pixel.png" } }));
    ASSERT_TRUE(catalog.add(
        { .id = assetID(3), .type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "missing.dds" } }));
    ASSERT_TRUE(catalog.add(
        { .id = assetID(4), .type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "truncated.dds" } }));
    ASSERT_TRUE(catalog.add(
        { .id = assetID(5), .type = hz::AssetType::Texture, .pSourcePath = "pixel.png", .location = { .pPath = "broken.dds" } }));
    hz::TextureAssetLoader loader(context, RD_OTHER_FILES);
    hz::AssetManager       assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Texture, loader.getLoader());
    const hz::AssetHandle canceled = assetManager.request(assetID(1));
    assetManager.update();
    assetManager.release(canceled);
    for (uint32_t i = 2; i <= 5; ++i)
    {
        const hz::AssetHandle invalid = assetManager.request(assetID(i));
        wait(assetManager, invalid);
        EXPECT_EQ(assetManager.getError(invalid), hz::AssetError::UploadFailed);
        EXPECT_EQ(assetManager.getTexture(invalid), nullptr);
    }
    assetManager.shutdown();
    EXPECT_EQ(assetManager.getCount(), 0u);
}

static const char kTypedScene[] = R"json({
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

static const VertexLayout kAssetTestLayout = {
    .attribs = { { .semantic = SEMANTIC_POSITION, .format = hz::Format::R32G32B32_SFLOAT, .binding = 2 } },
    .attribCount = 1,
};

class TypedAssetLiveTest: public TextureAssetLiveTest
{
protected:
    void writeScene(const char* text = kTypedScene, const char* path = "scene.sceneasset.json")
    {
        FileStream stream = {};
        ASSERT_TRUE(fsOpenStreamFromPath(RD_OTHER_FILES, path, FM_WRITE, &stream));
        EXPECT_EQ(fsWriteToStream(&stream, text, strlen(text)), strlen(text));
        fsCloseStream(&stream);
    }
    void writeGeometry(IndexType indexType = INDEX_TYPE_UINT16)
    {
        Geometry geometry = {};
        geometry.drawArgCount = 1;
        geometry.indexCount = 3;
        geometry.vertexCount = 3;
        geometry.indexType = indexType;
        const IndirectDrawIndexArguments draw = { .indexCount = 3, .instanceCount = 1 };
        const GeometryData               data = {};
        GeometryData::ShadowData         shadow = {};
        shadow.vertexStrides[SEMANTIC_POSITION] = 12;
        shadow.attributeCount[SEMANTIC_POSITION] = 3;
        const uint16_t indices16[] = { 0, 1, 2 };
        const uint32_t indices32[] = { 0, 1, 2 };
        const void*    pIndices = indexType == INDEX_TYPE_UINT16 ? (const void*)indices16 : indices32;
        const uint32_t indexBytes = indexType == INDEX_TYPE_UINT16 ? sizeof(indices16) : sizeof(indices32);
        const float    positions[] = { -1, 2, 3, 4, -5, 6, 0, 1, -2 };
        const uint32_t geometrySize = sizeof(geometry) + sizeof(draw);
        const uint32_t dataSize = sizeof(data);
        const uint32_t shadowSize = sizeof(shadow) + indexBytes + sizeof(positions);
        FileStream     stream = {};
        ASSERT_TRUE(fsOpenStreamFromPath(RD_OTHER_FILES, "scene.bin", FM_WRITE, &stream));
        fsWriteToStream(&stream, GEOMETRY_FILE_MAGIC_STR, sizeof(GEOMETRY_FILE_MAGIC_STR));
        fsWriteToStream(&stream, &geometrySize, sizeof(geometrySize));
        fsWriteToStream(&stream, &geometry, sizeof(geometry));
        fsWriteToStream(&stream, &draw, sizeof(draw));
        fsWriteToStream(&stream, &dataSize, sizeof(dataSize));
        fsWriteToStream(&stream, &data, sizeof(data));
        fsWriteToStream(&stream, &shadowSize, sizeof(shadowSize));
        fsWriteToStream(&stream, &shadow, sizeof(shadow));
        fsWriteToStream(&stream, pIndices, indexBytes);
        fsWriteToStream(&stream, positions, sizeof(positions));
        fsCloseStream(&stream);
    }
};

TEST_F(TypedAssetLiveTest, LoadsSharedDependencyGraphAndRetiresGeometryAfterGpuUse)
{
    writeTexture();
    writeScene();
    writeGeometry();
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(scene, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "TypedAssetTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = RD_OTHER_FILES });
    hz::AssetManager  assetManager(catalog, &context);
    loaders.registerLoaders(assetManager);
    const hz::AssetHandle root = assetManager.request(scene.getID());
    const hz::AssetHandle second = assetManager.request(scene.getMeshes().pData[1].id);
    wait(assetManager, root);
    ASSERT_EQ(assetManager.getState(root), hz::AssetState::Ready);
    EXPECT_EQ(assetManager.getCount(), 5u);
    const hz::AssetHandle first = assetManager.find(scene.getMeshes().pData[0].id);
    const hz::MeshAsset*  pFirst = assetManager.getMesh(first);
    const hz::MeshAsset*  pSecond = assetManager.getMesh(second);
    ASSERT_NE(pFirst, nullptr);
    ASSERT_NE(pSecond, nullptr);
    EXPECT_EQ(&pFirst->getIndexBuffer(), &pSecond->getIndexBuffer());
    EXPECT_EQ(&pFirst->getVertexBuffer(0), &pSecond->getVertexBuffer(0));
    EXPECT_EQ(pFirst->getVertexBufferCount(), 1u);
    EXPECT_EQ(pFirst->getVertexStride(0), 12u);
    EXPECT_EQ(pFirst->getIndexType(), INDEX_TYPE_UINT16);
    EXPECT_EQ(pFirst->getDraw(0).indexCount, 3u);
    const hz::MeshBounds& bounds = pFirst->getBounds(0);
    EXPECT_FLOAT_EQ(bounds.minimum[0], -1);
    EXPECT_FLOAT_EQ(bounds.minimum[1], -5);
    EXPECT_FLOAT_EQ(bounds.minimum[2], -2);
    EXPECT_FLOAT_EQ(bounds.maximum[0], 4);
    EXPECT_FLOAT_EQ(bounds.maximum[1], 2);
    EXPECT_FLOAT_EQ(bounds.maximum[2], 6);
    const hz::MaterialAsset* pMaterial = assetManager.getMaterial(assetManager.find(scene.getMaterials().pData[0].id));
    ASSERT_NE(pMaterial, nullptr);
    EXPECT_FLOAT_EQ(pMaterial->getDescription().roughnessFactor, 0.8f);
    ASSERT_NE(assetManager.getTexture(assetManager.find(pMaterial->getDescription().baseColorTexture)), nullptr);
    hz::GPUBuffer    readback = context.createBuffer({ .size = 36,
                                                       .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                       .startState = RESOURCE_STATE_COPY_DEST,
                                                       .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::CommandList& commands = context.acquireCommandList();
    commands.copyBuffer(readback, 0, pFirst->getVertexBuffer(0), 0, 36);
    assetManager.release(root);
    assetManager.release(second);
    assetManager.update();
    EXPECT_EQ(assetManager.getCount(), 5u);
    const hz::SubmitHandle submission = context.submit(commands);
    assetManager.shutdown();
    EXPECT_EQ(assetManager.getCount(), 0u);
    EXPECT_TRUE(context.isComplete(submission));
    const float* pVertices = (const float*)readback.get()->pCpuMappedAddress;
    EXPECT_FLOAT_EQ(pVertices[0], -1);
    EXPECT_FLOAT_EQ(pVertices[4], -5);
    EXPECT_FLOAT_EQ(pVertices[8], -2);
}

TEST_F(TypedAssetLiveTest, Loads32BitIndicesWithSmallVertexCounts)
{
    writeTexture();
    writeScene();
    writeGeometry(INDEX_TYPE_UINT32);
    hz::SceneAsset asset;
    ASSERT_TRUE(asset.parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(asset, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "TypedAsset32BitIndexTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = RD_OTHER_FILES });
    hz::AssetManager  assetManager(catalog, &context);
    loaders.registerLoaders(assetManager);
    const hz::AssetHandle handle = assetManager.request(asset.getMeshes().pData[0].id);
    wait(assetManager, handle);
    ASSERT_EQ(assetManager.getState(handle), hz::AssetState::Ready);
    const hz::MeshAsset* mesh = assetManager.getMesh(handle);
    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->getIndexType(), INDEX_TYPE_UINT32);
    const hz::MeshBounds& bounds = mesh->getBounds(0);
    const float           minimum[] = { -1, -5, -2 }, maximum[] = { 4, 2, 6 };
    for (uint32_t axis = 0; axis < 3; ++axis)
    {
        EXPECT_FLOAT_EQ(bounds.minimum[axis], minimum[axis]);
        EXPECT_FLOAT_EQ(bounds.maximum[axis], maximum[axis]);
    }
    const hz::GpuMeshRecord* gpuMesh = assetManager.getGpuTables().getMesh({ .index = handle.index, .generation = handle.generation });
    ASSERT_NE(gpuMesh, nullptr);
    EXPECT_EQ(gpuMesh->indexStride, 4u);
    hz::GPUBuffer    readback = context.createBuffer({ .size = 48,
                                                       .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                       .startState = RESOURCE_STATE_COPY_DEST,
                                                       .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::CommandList& commands = context.acquireCommandList();
    commands.copyBuffer(readback, 0, mesh->getIndexBuffer(), 0, 12);
    commands.copyBuffer(readback, 12, mesh->getVertexBuffer(0), 0, 36);
    assetManager.release(handle);
    const hz::SubmitHandle submission = context.submit(commands);
    assetManager.shutdown();
    EXPECT_TRUE(context.isComplete(submission));
    const uint32_t expectedIndices[] = { 0, 1, 2 };
    const float    expectedPositions[] = { -1, 2, 3, 4, -5, 6, 0, 1, -2 };
    const uint8_t* bytes = (const uint8_t*)readback.get()->pCpuMappedAddress;
    EXPECT_EQ(memcmp(bytes, expectedIndices, sizeof(expectedIndices)), 0);
    EXPECT_EQ(memcmp(bytes + sizeof(expectedIndices), expectedPositions, sizeof(expectedPositions)), 0);
}

TEST_F(AssetManagerTest, WorldBindingsRetainSharedMeshesAndReleaseRemovedComponents)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(
        catalog.add({ .id = assetID(1), .type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Mesh, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity a = world.createEntity(), b = world.createEntity();
    hz::MeshRenderer renderer = { .mesh = assetID(1) };
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    hz::WorldAssetBindings bindings(world, assetManager);
    bindings.update();
    ASSERT_EQ(bindings.getCount(), 2u);
    const hz::AssetHandle mesh = bindings.get(a)->mesh;
    EXPECT_EQ(bindings.get(b)->mesh, mesh);
    assetManager.update();
    bindings.update();
    EXPECT_EQ(bindings.get(a)->state, hz::AssetBindingState::Pending);
    EXPECT_EQ(loader.begun, 1u);
    const MemoryTrackingStats before = memGetTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
        bindings.update();
    const MemoryTrackingStats after = memGetTrackingStats();
    if (before.trackingEnabled)
    {
        EXPECT_EQ(before.totalAllocationCount, after.totalAllocationCount);
        EXPECT_EQ(before.reallocationCount, after.reallocationCount);
    }
    ASSERT_TRUE(world.removeComponent(a, hz::SceneTypes::meshRenderer));
    bindings.update();
    EXPECT_EQ(bindings.get(a), nullptr);
    EXPECT_EQ(bindings.getCount(), 1u);
    EXPECT_EQ(assetManager.getState(mesh), hz::AssetState::LoadingCPU);
    ASSERT_TRUE(world.destroyEntity(b));
    bindings.update();
    EXPECT_EQ(bindings.getCount(), 0u);
    EXPECT_EQ(assetManager.getState(mesh), hz::AssetState::Retiring);
    loader.jobs[1]->ready = true;
    assetManager.update();
    EXPECT_EQ(assetManager.getCount(), 0u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1 }));
}

TEST_F(AssetManagerTest, WorldBindingsSwapMeshesWithoutRetiringSharedAssets)
{
    hz::AssetCatalog catalog;
    for (uint32_t i = 1; i <= 2; ++i)
        ASSERT_TRUE(catalog.add(
            { .id = assetID(i), .type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Mesh, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity a = world.createEntity(), b = world.createEntity();
    hz::MeshRenderer first = { .mesh = assetID(1) }, second = { .mesh = assetID(2) };
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &first));
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &second));
    hz::WorldAssetBindings bindings(world, assetManager);
    bindings.update();
    const hz::AssetHandle meshA = bindings.get(a)->mesh, meshB = bindings.get(b)->mesh;
    assetManager.update();
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &second));
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &first));
    bindings.update();
    EXPECT_EQ(bindings.get(a)->mesh, meshB);
    EXPECT_EQ(bindings.get(b)->mesh, meshA);
    EXPECT_EQ(assetManager.getState(meshA), hz::AssetState::LoadingCPU);
    EXPECT_EQ(assetManager.getState(meshB), hz::AssetState::LoadingCPU);
    EXPECT_EQ(loader.begun, 2u);

    ASSERT_TRUE(world.destroyEntity(a));
    bindings.update();
    const hz::Entity replacement = world.createEntity();
    EXPECT_NE(replacement, a);
    ASSERT_TRUE(world.setComponent(replacement, hz::SceneTypes::meshRenderer, &second));
    bindings.update();
    EXPECT_EQ(bindings.get(a), nullptr);
    EXPECT_EQ(bindings.get(replacement)->state, hz::AssetBindingState::Pending);
    EXPECT_FALSE(bindings.get(replacement)->mesh.isValid());
    loader.jobs[2]->ready = true;
    assetManager.update();
    bindings.update();
    EXPECT_TRUE(bindings.get(replacement)->mesh.isValid());
    EXPECT_NE(bindings.get(replacement)->mesh, meshB);
    bindings.clear();
    EXPECT_EQ(bindings.getCount(), 0u);
    EXPECT_EQ(assetManager.getState(meshA), hz::AssetState::Retiring);
}

TEST_F(AssetManagerTest, RenderSceneKeepsPendingSlotsAndRejectsReusedGenerations)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(
        catalog.add({ .id = assetID(1), .type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Mesh, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, assetManager);
    const hz::MeshRenderer renderer = { .mesh = assetID(1) };
    const hz::Entity       a = world.createEntity(), b = world.createEntity();
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    scene.update();
    assetManager.update();
    const hz::RenderPrimitiveID first = scene.findPrimitive(a), second = scene.findPrimitive(b);
    ASSERT_TRUE(first.isValid());
    ASSERT_TRUE(second.isValid());
    EXPECT_EQ(scene.getActivePrimitives().count, 2u);
    EXPECT_EQ(scene.getActiveInstances().count, 2u);
    EXPECT_EQ(scene.getActiveMeshBatches().count, 0u);
    EXPECT_EQ(scene.getPrimitive(first)->instanceCount, 0u);
    ASSERT_EQ(scene.getChanges().instanceIndices.count, 1u);
    EXPECT_EQ(scene.getChanges().instanceIndices.pData[0].count, 2u);
    for (uint32_t index : scene.getInstanceIndices())
        EXPECT_EQ(index, UINT32_MAX);
    const hz::RenderInstanceID instance = scene.getInstances().pData[first.index].id;
    const uint64_t             revision = scene.getRevision();
    const MemoryTrackingStats  before = memGetTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
        scene.update();
    const MemoryTrackingStats after = memGetTrackingStats();
    if (before.trackingEnabled)
    {
        EXPECT_EQ(before.totalAllocationCount, after.totalAllocationCount);
        EXPECT_EQ(before.reallocationCount, after.reallocationCount);
    }
    EXPECT_EQ(scene.getRevision(), revision);
    EXPECT_EQ(scene.getChanges().primitives.ranges.count, 0u);
    EXPECT_EQ(scene.getChanges().instances.ranges.count, 0u);
    ASSERT_TRUE(world.removeComponent(a, hz::SceneTypes::meshRenderer));
    scene.update();
    EXPECT_EQ(scene.getPrimitive(first), nullptr);
    EXPECT_EQ(scene.getInstance(instance), nullptr);
    EXPECT_FALSE(scene.findPrimitive(a).isValid());
    EXPECT_EQ(scene.findPrimitive(b), second);
    ASSERT_EQ(scene.getChanges().primitives.removed.count, 1u);
    EXPECT_EQ(scene.getChanges().primitives.removed.pData[0], first.index);
    EXPECT_TRUE(scene.getChanges().primitives.activeList);
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    scene.update();
    const hz::RenderPrimitiveID replacement = scene.findPrimitive(a);
    EXPECT_EQ(replacement.index, first.index);
    EXPECT_NE(replacement.generation, first.generation);
    EXPECT_EQ(scene.getPrimitive(first), nullptr);
    EXPECT_EQ(scene.getInstance(instance), nullptr);
    ASSERT_TRUE(world.destroyEntity(b));
    scene.update();
    EXPECT_EQ(scene.getPrimitive(second), nullptr);
    EXPECT_EQ(scene.getActivePrimitives().count, 1u);
    EXPECT_EQ(scene.getPrimitives().count, 2u);
}

TEST_F(AssetManagerTest, RenderSceneMergesSparseTransformChangesWithoutAllocating)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(
        catalog.add({ .id = assetID(1), .type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Mesh, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, assetManager);
    hz::Array<hz::Entity>  entities;
    const hz::MeshRenderer renderer = { .mesh = assetID(1) };
    for (uint32_t i = 0; i < 40; ++i)
    {
        const hz::Entity         entity = world.createEntity();
        const hz::LocalTransform transform;
        ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::meshRenderer, &renderer));
        ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
        entities.pushBack(entity);
    }
    scene.update();
    assetManager.update();
    world.commitTransforms();
    scene.update();
    const MemoryTrackingStats before = memGetTrackingStats();
    for (hz::Entity entity : entities)
        if ((scene.findPrimitive(entity).index % 2) == 0)
        {
            const hz::LocalTransform transform = { .translation = Vector3(2, 0, 0) };
            ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
        }
    scene.update();
    const MemoryTrackingStats after = memGetTrackingStats();
    if (before.trackingEnabled)
    {
        EXPECT_EQ(before.totalAllocationCount, after.totalAllocationCount);
        EXPECT_EQ(before.reallocationCount, after.reallocationCount);
    }
    ASSERT_EQ(scene.getChanges().instances.ranges.count, 20u);
    for (uint32_t i = 0; i < 20; ++i)
    {
        EXPECT_EQ(scene.getChanges().instances.ranges.pData[i].first, i * 2);
        EXPECT_EQ(scene.getChanges().instances.ranges.pData[i].count, 1u);
    }
    EXPECT_FALSE(scene.getChanges().instances.activeList);
    EXPECT_EQ(scene.getChanges().primitives.ranges.count, 0u);
    EXPECT_EQ(scene.getChanges().instanceIndices.count, 0u);
}

TEST_F(AssetManagerTest, RenderScenePendingUploadsAccumulateAcrossUpdates)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(
        catalog.add({ .id = assetID(1), .type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Mesh, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, assetManager);
    const hz::Entity       entity = world.createEntity();
    const hz::MeshRenderer renderer = { .mesh = assetID(1) };
    hz::LocalTransform     transform;
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
    scene.update();
    const hz::RenderSceneUploads initial = scene.getPendingUploads();
    EXPECT_EQ(initial.primitives.count, 1u);
    EXPECT_EQ(initial.instances.count, 1u);
    EXPECT_EQ(initial.instanceIndices.count, 1u);
    EXPECT_TRUE(initial.activePrimitives);
    EXPECT_TRUE(initial.activeInstances);
    EXPECT_FALSE(initial.activeMeshBatches);
    for (uint32_t i = 1; i <= 4; ++i)
    {
        transform.translation = Vector3((float)i, 0, 0);
        ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
        scene.update();
    }
    // getChanges() covers only the latest update; pending uploads keep everything not yet acknowledged.
    EXPECT_EQ(scene.getChanges().primitives.ranges.count, 0u);
    EXPECT_FALSE(scene.getChanges().primitives.activeList);
    const hz::RenderSceneUploads moved = scene.getPendingUploads();
    ASSERT_EQ(moved.instances.count, 1u);
    EXPECT_EQ(moved.instances.pData[0].first, 0u);
    EXPECT_EQ(moved.instances.pData[0].count, 1u);
    EXPECT_EQ(moved.primitives.count, 1u);
    EXPECT_TRUE(moved.activePrimitives);
    ASSERT_TRUE(world.removeComponent(entity, hz::SceneTypes::meshRenderer));
    scene.update();
    const hz::RenderSceneUploads removal = scene.getPendingUploads();
    EXPECT_TRUE(removal.activePrimitives);
    EXPECT_TRUE(removal.activeInstances);
    EXPECT_FALSE(removal.activeMeshBatches);
    EXPECT_EQ(removal.primitives.count, 1u);
    EXPECT_EQ(removal.instances.count, 1u);
    EXPECT_EQ(removal.instanceIndices.count, 1u);
}

TEST_F(AssetManagerTest, RenderSceneExtractsLightsAndCamerasWithStableSlotsAndNoFrameAllocation)
{
    hz::AssetCatalog catalog;
    hz::AssetManager assetManager(catalog);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World       world(types);
    hz::RenderScene scene(world, assetManager);
    scene.reserve(0, 0, 9, 9);
    const hz::Entity parent = world.createEntity();
    hz::LocalMatrix  parentMatrix = { .value = Matrix4::translation(Vector3(10, 20, 30)) * Matrix4::scale(Vector3(2, 3, 4)) };
    parentMatrix.value.setElem(1, 0, 0.5f);
    ASSERT_TRUE(world.setComponent(parent, hz::SceneTypes::localMatrix, &parentMatrix));
    hz::FixedArray<hz::Entity, 9> entities;
    hz::Light                     light = { .color = Vector3(0.1f, 0.2f, 0.3f),
                                            .intensity = 5,
                                            .range = 10,
                                            .innerConeAngle = 0.2f,
                                            .outerConeAngle = 0.4f,
                                            .exposure = 2,
                                            .angularDiameter = 0.05f,
                                            .width = 6,
                                            .height = 7,
                                            .radius = 8,
                                            .length = 9,
                                            .colorTemperature = 3200,
                                            .enableColorTemperature = true,
                                            .normalize = true,
                                            .twoSided = true };
    hz::Camera                    lens = { .verticalFov = 0.8f, .nearPlane = 0.2f, .farPlane = 500 };
    hz::LocalTransform            transform = { .translation = Vector3(1, 2, 3) };
    for (uint32_t i = 0; i < 9; ++i)
    {
        entities[i] = world.createPersistentEntity({ .high = 2, .low = i + 1 });
        light.type = (hz::LightType)i;
        ASSERT_TRUE(world.setComponent(entities[i], hz::SceneTypes::light, &light));
        ASSERT_TRUE(world.setComponent(entities[i], hz::SceneTypes::camera, &lens));
        ASSERT_TRUE(world.setComponent(entities[i], hz::SceneTypes::localTransform, &transform));
        ASSERT_TRUE(world.setParent(entities[i], parent, false));
    }
    scene.update();
    EXPECT_EQ(scene.getActiveLights().count, 9u);
    EXPECT_EQ(scene.getActiveCameras().count, 9u);
    for (uint32_t i = 0; i < 9; ++i)
    {
        const hz::RenderLight* extracted = scene.getLight(scene.findLight(entities[i]));
        ASSERT_NE(extracted, nullptr);
        EXPECT_EQ(extracted->entityGuid, world.getEntityGuid(entities[i]));
        EXPECT_EQ(extracted->type, (hz::LightType)i);
        EXPECT_EQ(extracted->ready, i == 8 ? 0u : 1u);
        EXPECT_FLOAT_EQ(extracted->color[2], 0.3f);
        EXPECT_FLOAT_EQ(extracted->intensity, 5);
        EXPECT_FLOAT_EQ(extracted->range, 10);
        EXPECT_FLOAT_EQ(extracted->innerConeAngle, 0.2f);
        EXPECT_FLOAT_EQ(extracted->outerConeAngle, 0.4f);
        EXPECT_FLOAT_EQ(extracted->exposure, 2);
        EXPECT_FLOAT_EQ(extracted->angularDiameter, 0.05f);
        EXPECT_FLOAT_EQ(extracted->width, 6);
        EXPECT_FLOAT_EQ(extracted->height, 7);
        EXPECT_FLOAT_EQ(extracted->radius, 8);
        EXPECT_FLOAT_EQ(extracted->length, 9);
        EXPECT_FLOAT_EQ(extracted->colorTemperature, 3200);
        EXPECT_EQ(extracted->enableColorTemperature, 1u);
        EXPECT_EQ(extracted->normalize, 1u);
        EXPECT_EQ(extracted->twoSided, 1u);
        EXPECT_FLOAT_EQ((float)extracted->transform.getElem(1, 0), 0.5f);
        EXPECT_FLOAT_EQ((float)extracted->transform.getElem(3, 0), 13);
        const hz::RenderCamera* camera = scene.getCamera(scene.findCamera(entities[i]));
        ASSERT_NE(camera, nullptr);
        EXPECT_EQ(camera->entityGuid, extracted->entityGuid);
        EXPECT_FLOAT_EQ((float)camera->current.getElem(3, 2), 42);
        EXPECT_FLOAT_EQ((float)camera->previous.getElem(3, 2), 42);
        EXPECT_FLOAT_EQ(camera->lens.verticalFov, 0.8f);
        EXPECT_FLOAT_EQ(camera->lens.nearPlane, 0.2f);
        EXPECT_FLOAT_EQ(camera->lens.farPlane, 500);
    }
    const uint64_t            initial = scene.getRevision();
    const MemoryTrackingStats before = memGetTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
        scene.update();
    EXPECT_EQ(scene.getRevision(), initial);
    // Change every other entity, then keep nonadjacent camera/light ranges separate.
    transform.translation = Vector3(2, 3, 4);
    lens.verticalFov = 0.6f;
    for (uint32_t i = 0; i < 9; i += 2)
    {
        ASSERT_TRUE(world.setComponent(entities[i], hz::SceneTypes::localTransform, &transform));
        ASSERT_TRUE(world.setComponent(entities[i], hz::SceneTypes::camera, &lens));
    }
    scene.update();
    const hz::RenderSceneChanges changes = scene.getChanges();
    EXPECT_GT(scene.getRevision(), initial);
    EXPECT_EQ(changes.lights.ranges.count, 5u);
    EXPECT_EQ(changes.cameras.ranges.count, 5u);
    EXPECT_FALSE(changes.lights.activeList);
    EXPECT_FALSE(changes.cameras.activeList);
    const MemoryTrackingStats after = memGetTrackingStats();
    if (before.trackingEnabled)
    {
        EXPECT_EQ(before.totalAllocationCount, after.totalAllocationCount);
        EXPECT_EQ(before.reallocationCount, after.reallocationCount);
    }
    const hz::RenderLightID  oldLight = scene.findLight(entities[0]);
    const hz::RenderCameraID oldCamera = scene.findCamera(entities[0]);
    ASSERT_TRUE(world.removeComponent(entities[0], hz::SceneTypes::light));
    ASSERT_TRUE(world.removeComponent(entities[0], hz::SceneTypes::camera));
    scene.update();
    EXPECT_EQ(scene.getLight(oldLight), nullptr);
    EXPECT_EQ(scene.getCamera(oldCamera), nullptr);
    EXPECT_FALSE(scene.findLight(entities[0]).isValid());
    EXPECT_FALSE(scene.findCamera(entities[0]).isValid());
    EXPECT_TRUE(scene.getChanges().lights.activeList);
    EXPECT_TRUE(scene.getChanges().cameras.activeList);
    ASSERT_TRUE(world.setComponent(entities[0], hz::SceneTypes::light, &light));
    ASSERT_TRUE(world.setComponent(entities[0], hz::SceneTypes::camera, &lens));
    scene.update();
    EXPECT_EQ(scene.findLight(entities[0]).index, oldLight.index);
    EXPECT_NE(scene.findLight(entities[0]).generation, oldLight.generation);
    EXPECT_EQ(scene.findCamera(entities[0]).index, oldCamera.index);
    EXPECT_NE(scene.findCamera(entities[0]).generation, oldCamera.generation);
    const hz::RenderCamera* replacement = scene.getCamera(scene.findCamera(entities[0]));
    EXPECT_FLOAT_EQ(replacement->previousLens.verticalFov, 0.6f);
    EXPECT_FLOAT_EQ((float)replacement->previous.getElem(3, 0), 15.5f);
    ASSERT_TRUE(world.destroySubtree(parent));
    scene.update();
    EXPECT_EQ(scene.getActiveLights().count, 0u);
    EXPECT_EQ(scene.getActiveCameras().count, 0u);
}

TEST_F(AssetManagerTest, RenderSceneEmissionBindingsHandlePendingFailureAndLeaseHandoffs)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.add(
        { .id = assetID(1), .type = hz::AssetType::Texture, .pSourcePath = "light.png", .location = { .pPath = "light.dds" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Texture, loader.getLoader());
    add(catalog, 2);
    assetManager.setLoader(hz::AssetType::Scene, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    hz::RenderScene  scene(world, assetManager);
    const hz::Entity a = world.createEntity(), b = world.createEntity();
    hz::Light        light = { .type = hz::LightType::Dome, .texture = assetID(1) };
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::light, &light));
    scene.update();
    const hz::RenderLightID id = scene.findLight(a);
    const hz::AssetHandle   handle = assetManager.find(light.texture);
    ASSERT_TRUE(handle.isValid());
    assetManager.update();
    EXPECT_EQ(scene.getLight(id)->ready, 0u);
    EXPECT_FALSE(scene.getLight(id)->texture.isValid());
    light.type = hz::LightType::Point;
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::light, &light));
    scene.update();
    EXPECT_EQ(scene.getLight(scene.findLight(b))->ready, 1u);
    EXPECT_FALSE(scene.getLight(scene.findLight(b))->texture.isValid());
    // Move the only lease from a to b in one update, without retiring the texture.
    ASSERT_TRUE(world.removeComponent(a, hz::SceneTypes::light));
    light.type = hz::LightType::Rect;
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::light, &light));
    scene.update();
    assetManager.update();
    EXPECT_EQ(assetManager.find(light.texture), handle);
    EXPECT_EQ(loader.begun, 1u);
    loader.jobs[1]->failed = true;
    loader.jobs[1]->ready = true;
    assetManager.update();
    scene.update();
    EXPECT_EQ(scene.getLight(scene.findLight(b))->ready, 0u);
    for (uint32_t i = 0; i < 20; ++i)
    {
        assetManager.update();
        scene.update();
    }
    EXPECT_EQ(loader.begun, 1u);
    light.texture = assetID(999);
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::light, &light));
    scene.update();
    assetManager.update();
    EXPECT_FALSE(assetManager.find(assetID(1)).isValid());
    EXPECT_EQ(scene.getLight(scene.findLight(b))->ready, 0u);
    scene.update();
    light.texture = {};
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::light, &light));
    scene.update();
    EXPECT_EQ(scene.getLight(scene.findLight(b))->ready, 1u);
    EXPECT_EQ(assetManager.getCount(), 0u);
    // A Ready payload of another type must never become a GPU texture reference.
    loader.ready = true;
    light.texture = assetID(2);
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::light, &light));
    scene.update();
    assetManager.update();
    scene.update();
    EXPECT_EQ(assetManager.getState(assetManager.find(light.texture)), hz::AssetState::Ready);
    EXPECT_EQ(scene.getLight(scene.findLight(b))->ready, 0u);
    EXPECT_FALSE(scene.getLight(scene.findLight(b))->texture.isValid());
}

TEST_F(AssetManagerTest, WorldBindingsKeepFailedRequestsIdleUntilRebuilt)
{
    hz::AssetCatalog catalog;
    TestAssetLoader  loader;
    loader.ready = false;
    hz::AssetManager assetManager(catalog);
    assetManager.setLoader(hz::AssetType::Mesh, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World              world(types);
    const hz::Entity       entity = world.createEntity();
    const hz::MeshRenderer renderer = { .mesh = assetID(1) };
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::meshRenderer, &renderer));
    hz::WorldAssetBindings bindings(world, assetManager);
    bindings.update();
    ASSERT_EQ(bindings.get(entity)->state, hz::AssetBindingState::Failed);
    ASSERT_TRUE(
        catalog.add({ .id = assetID(1), .type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    const MemoryTrackingStats before = memGetTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
        bindings.update();
    const MemoryTrackingStats after = memGetTrackingStats();
    if (before.trackingEnabled)
    {
        EXPECT_EQ(before.totalAllocationCount, after.totalAllocationCount);
        EXPECT_EQ(before.reallocationCount, after.reallocationCount);
    }
    EXPECT_EQ(assetManager.getCount(), 0u);
    bindings.clear();
    bindings.update();
    EXPECT_EQ(bindings.get(entity)->state, hz::AssetBindingState::Pending);
    EXPECT_TRUE(bindings.get(entity)->mesh.isValid());
    EXPECT_EQ(assetManager.getCount(), 1u);
}

TEST_F(TypedAssetLiveTest, WorldBindingsResolveSubmeshMaterialsAndDeferReleasedPayloads)
{
    const std::string firstSubmesh = R"({"id":"8000000000000001", "draw":0, "material":"00000000-0000-4000-8000-000000000003"})";
    const std::string secondSubmesh = R"({"id":"8000000000000003", "draw":0, "material":"00000000-0000-4000-8000-000000000003"})";
    std::string       sceneText = kTypedScene;
    sceneText.replace(sceneText.find(firstSubmesh), firstSubmesh.size(), firstSubmesh + "," + secondSubmesh);
    std::string       overrideText = kTypedScene;
    const std::string originalID = "00000000-0000-4000-8000-000000000003";
    const std::string overrideID = "00000000-0000-4000-8000-000000000006";
    for (size_t offset = overrideText.find(originalID); offset != std::string::npos; offset = overrideText.find(originalID))
        overrideText.replace(offset, originalID.size(), overrideID);
    writeTexture();
    writeScene(sceneText.c_str());
    writeScene(overrideText.c_str(), "override.sceneasset.json");
    writeGeometry();
    hz::SceneAsset scene, overrideScene;
    ASSERT_TRUE(scene.parse({ sceneText.data(), (uint32_t)sceneText.size() }));
    ASSERT_TRUE(overrideScene.parse({ overrideText.data(), (uint32_t)overrideText.size() }));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(scene, "scene.sceneasset.json"));
    const hz::AssetID alternate = overrideScene.getMaterials().pData[0].id;
    ASSERT_TRUE(catalog.add({ .id = alternate,
                              .type = hz::AssetType::Material,
                              .pSourcePath = "scene.gltf",
                              .location = { .pPath = "override.sceneasset.json", .elementIndex = 0 },
                              .dependencies = { scene.getTextures().pData[0].id } }));
    hz::RenderContext context({ .pAppName = "WorldAssetBindingsTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = RD_OTHER_FILES });
    hz::AssetManager  assetManager(catalog, &context);
    loaders.registerLoaders(assetManager);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity a = world.createEntity(), b = world.createEntity();
    hz::MeshRenderer renderer = { .mesh = scene.getMeshes().pData[0].id };
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    renderer.overrides.pushBack({ .submesh = UINT64_C(0x8000000000000003), .material = alternate });
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    hz::WorldAssetBindings bindings(world, assetManager);
    bindings.update();
    const hz::AssetHandle mesh = bindings.get(a)->mesh;
    EXPECT_EQ(bindings.get(a)->state, hz::AssetBindingState::Pending);
    wait(assetManager, mesh);
    bindings.update();
    const hz::AssetHandle overrideMaterial = assetManager.find(alternate);
    ASSERT_TRUE(overrideMaterial.isValid());
    wait(assetManager, overrideMaterial);
    bindings.update();
    ASSERT_EQ(bindings.get(a)->state, hz::AssetBindingState::Ready);
    ASSERT_EQ(bindings.get(b)->state, hz::AssetBindingState::Ready);
    ASSERT_EQ(bindings.get(a)->materials.count, 2u);
    EXPECT_EQ(bindings.get(a)->materials.pData[0].asset, scene.getMaterials().pData[0].id);
    EXPECT_EQ(bindings.get(a)->materials.pData[1].asset, alternate);
    EXPECT_EQ(bindings.get(b)->materials.pData[1].asset, scene.getMaterials().pData[0].id);
    EXPECT_EQ(bindings.get(a)->materials.pData[1].handle, overrideMaterial);

    const MemoryTrackingStats before = memGetTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
        bindings.update();
    const MemoryTrackingStats after = memGetTrackingStats();
    if (before.trackingEnabled)
    {
        EXPECT_EQ(before.totalAllocationCount, after.totalAllocationCount);
        EXPECT_EQ(before.reallocationCount, after.reallocationCount);
    }

    // An open command list may reference any ready payload, so released payloads stay alive until it completes.
    hz::CommandList& pending = context.acquireCommandList();
    // Reverse authoring order; resolution follows stable SubmeshID, not override array position.
    renderer.overrides.pushBack({ .submesh = UINT64_C(0x8000000000000001), .material = alternate });
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    bindings.update();
    ASSERT_EQ(bindings.get(a)->state, hz::AssetBindingState::Ready);
    EXPECT_EQ(bindings.get(a)->materials.pData[0].handle, overrideMaterial);
    EXPECT_EQ(bindings.get(a)->materials.pData[1].handle, overrideMaterial);

    const hz::MeshRenderer pendingRenderer = { .mesh = scene.getMeshes().pData[1].id };
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &pendingRenderer));
    bindings.update();
    EXPECT_EQ(bindings.get(a)->state, hz::AssetBindingState::Pending);
    EXPECT_EQ(bindings.get(a)->materials.count, 0u);
    assetManager.update();
    EXPECT_EQ(assetManager.getState(overrideMaterial), hz::AssetState::Retiring);

    renderer.overrides[0].submesh = 99;
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    bindings.update();
    EXPECT_EQ(bindings.get(a)->state, hz::AssetBindingState::Failed);
    renderer.overrides[0].submesh = renderer.overrides[1].submesh;
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    bindings.update();
    EXPECT_EQ(bindings.get(a)->state, hz::AssetBindingState::Failed);
    renderer.overrides.clear();
    renderer.overrides.pushBack({ .submesh = UINT64_C(0x8000000000000001), .material = scene.getTextures().pData[0].id });
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    bindings.update();
    EXPECT_EQ(bindings.get(a)->state, hz::AssetBindingState::Failed);

    ASSERT_TRUE(world.removeComponent(a, hz::SceneTypes::meshRenderer));
    ASSERT_TRUE(world.destroyEntity(b));
    bindings.update();
    assetManager.update();
    EXPECT_EQ(bindings.getCount(), 0u);
    EXPECT_EQ(assetManager.getState(mesh), hz::AssetState::Retiring);
    EXPECT_EQ(assetManager.getState(overrideMaterial), hz::AssetState::Retiring);
    context.wait(context.submit(pending));
    assetManager.update();
    EXPECT_EQ(assetManager.getCount(), 0u);
}

TEST_F(TypedAssetLiveTest, RenderSceneExtractsSubmeshesBoundsHistoryAndMaterialChanges)
{
    const std::string submesh = R"({"id":"8000000000000001", "draw":0, "material":"00000000-0000-4000-8000-000000000003"})";
    const std::string extra = R"({"id":"8000000000000003", "draw":0, "material":"00000000-0000-4000-8000-000000000003"})";
    std::string       text = kTypedScene;
    text.replace(text.find(submesh), submesh.size(), submesh + "," + extra);
    std::string       alternateText = kTypedScene;
    const std::string originalID = "00000000-0000-4000-8000-000000000003";
    const std::string alternateID = "00000000-0000-4000-8000-000000000006";
    for (size_t offset = alternateText.find(originalID); offset != std::string::npos; offset = alternateText.find(originalID))
        alternateText.replace(offset, originalID.size(), alternateID);
    writeTexture();
    writeGeometry();
    writeScene(text.c_str());
    writeScene(alternateText.c_str(), "alternate.sceneasset.json");
    hz::SceneAsset assets, alternateAssets;
    ASSERT_TRUE(assets.parse({ text.data(), (uint32_t)text.size() }));
    ASSERT_TRUE(alternateAssets.parse({ alternateText.data(), (uint32_t)alternateText.size() }));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(assets, "scene.sceneasset.json"));
    const hz::AssetID alternate = alternateAssets.getMaterials().pData[0].id;
    ASSERT_TRUE(catalog.add({ .id = alternate,
                              .type = hz::AssetType::Material,
                              .pSourcePath = "scene.gltf",
                              .location = { .pPath = "alternate.sceneasset.json", .elementIndex = 0 },
                              .dependencies = { assets.getTextures().pData[0].id } }));
    hz::RenderContext context({ .pAppName = "RenderSceneTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = RD_OTHER_FILES });
    hz::AssetManager  assetManager(catalog);
    loaders.registerLoaders(assetManager);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    hz::RenderScene  scene(world, assetManager);
    const hz::Guid   identity = { .high = 2, .low = 1 };
    const hz::Entity a = world.createPersistentEntity(identity), b = world.createEntity();
    hz::MeshRenderer renderer = { .mesh = assets.getMeshes().pData[0].id };
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    hz::LocalMatrix local;
    local.value = Matrix4::scale(Vector3(-2, 3, 0.5f));
    local.value.setElem(1, 0, 1.0f); // Shear X by Y.
    local.value.setTranslation(Vector3(10, 20, 30));
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::localMatrix, &local));
    scene.update();
    const hz::RenderPrimitiveID primitive = scene.findPrimitive(a);
    wait(assetManager, assetManager.find(renderer.mesh));
    scene.update();
    ASSERT_EQ(scene.getPrimitive(primitive)->instanceCount, 1u);
    EXPECT_EQ(scene.findPrimitive(a), primitive);
    ASSERT_EQ(scene.getActiveMeshBatches().count, 4u);
    ASSERT_EQ(scene.getChanges().meshBatches.created.count, 4u);
    const uint32_t             instanceIndex = scene.getInstanceIndices().pData[scene.getPrimitive(primitive)->instanceOffset];
    const hz::RenderInstanceID instanceID = scene.getInstances().pData[instanceIndex].id;
    const hz::RenderInstance*  instance = scene.getInstance(instanceID);
    ASSERT_NE(instance, nullptr);
    EXPECT_EQ(instance->entityGuid, identity);
    EXPECT_EQ(instance->primitive, primitive);
    EXPECT_FLOAT_EQ(instance->bounds.minimum[0], -3);
    EXPECT_FLOAT_EQ(instance->bounds.maximum[0], 14);
    EXPECT_FLOAT_EQ(instance->bounds.minimum[1], 5);
    EXPECT_FLOAT_EQ(instance->bounds.maximum[1], 26);
    EXPECT_FLOAT_EQ(instance->bounds.minimum[2], 29);
    EXPECT_FLOAT_EQ(instance->bounds.maximum[2], 33);
    hz::Array<hz::RenderMeshBatchID> meshBatchIDs;
    for (uint32_t index : scene.getActiveMeshBatches())
        if (scene.getMeshBatches().pData[index].primitive == primitive)
            meshBatchIDs.pushBack(scene.getMeshBatches().pData[index].id);
    ASSERT_EQ(meshBatchIDs.size(), 2u);
    EXPECT_EQ(scene.getMeshBatch(meshBatchIDs[0])->submesh, UINT64_C(0x8000000000000001));
    EXPECT_EQ(scene.getMeshBatch(meshBatchIDs[1])->submesh, UINT64_C(0x8000000000000003));
    const hz::GpuMaterialID original = scene.getMeshBatch(meshBatchIDs[0])->material;

    world.commitTransforms();
    scene.update();
    const uint64_t            revision = scene.getRevision();
    const MemoryTrackingStats before = memGetTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
        scene.update();
    const MemoryTrackingStats after = memGetTrackingStats();
    if (before.trackingEnabled)
    {
        EXPECT_EQ(before.totalAllocationCount, after.totalAllocationCount);
        EXPECT_EQ(before.reallocationCount, after.reallocationCount);
    }
    EXPECT_EQ(scene.getRevision(), revision);
    EXPECT_EQ(scene.getChanges().meshBatches.ranges.count, 0u);
    local.value.setTranslation(Vector3(11, 20, 30));
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::localMatrix, &local));
    scene.update();
    EXPECT_FLOAT_EQ((float)scene.getInstance(instanceID)->current.getElem(3, 0), 11);
    EXPECT_FLOAT_EQ((float)scene.getInstance(instanceID)->previous.getElem(3, 0), 11);
    EXPECT_EQ(scene.getChanges().primitives.ranges.count, 0u);
    EXPECT_EQ(scene.getChanges().meshBatches.ranges.count, 0u);
    ASSERT_EQ(scene.getChanges().instances.ranges.count, 1u);
    EXPECT_EQ(scene.getChanges().instances.ranges.pData[0].first, instanceIndex);
    EXPECT_EQ(scene.getChanges().instances.ranges.pData[0].count, 1u);
    scene.update();
    EXPECT_FLOAT_EQ((float)scene.getInstance(instanceID)->previous.getElem(3, 0), 11);
    scene.resetHistory(a);
    scene.update();
    EXPECT_FLOAT_EQ((float)scene.getInstance(instanceID)->previous.getElem(3, 0), 11);
    EXPECT_EQ(scene.getChanges().instances.ranges.count, 0u);

    renderer.overrides.pushBack({ .submesh = UINT64_C(0x8000000000000003), .material = alternate });
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    scene.update();
    EXPECT_EQ(scene.getPrimitive(primitive)->instanceCount, 0u);
    EXPECT_NE(scene.getMeshBatch(meshBatchIDs[0]), nullptr);
    EXPECT_NE(scene.getMeshBatch(meshBatchIDs[1]), nullptr);
    EXPECT_FALSE(scene.getMeshBatch(meshBatchIDs[1])->material.isValid());
    wait(assetManager, assetManager.find(alternate));
    scene.update();
    EXPECT_EQ(scene.getPrimitive(primitive)->instanceCount, 1u);
    EXPECT_EQ(scene.getMeshBatch(meshBatchIDs[0])->material, original);
    const hz::AssetHandle alternateHandle = assetManager.find(alternate);
    EXPECT_EQ(scene.getMeshBatch(meshBatchIDs[1])->material,
              (hz::GpuMaterialID{ .index = alternateHandle.index, .generation = alternateHandle.generation }));
    EXPECT_EQ(scene.getChanges().meshBatches.created.count, 0u);
    renderer.overrides.clear();
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    scene.update();
    EXPECT_EQ(scene.getMeshBatch(meshBatchIDs[1])->material, original);
    EXPECT_EQ(scene.getChanges().primitives.ranges.count, 0u);
    ASSERT_EQ(scene.getChanges().meshBatches.ranges.count, 1u);
    EXPECT_EQ(scene.getChanges().meshBatches.ranges.pData[0].first, meshBatchIDs[1].index);
    EXPECT_EQ(scene.getChanges().meshBatches.ranges.pData[0].count, 1u);

    ASSERT_TRUE(world.destroyEntity(a));
    scene.update();
    EXPECT_EQ(scene.getPrimitive(primitive), nullptr);
    EXPECT_EQ(scene.getInstance(instanceID), nullptr);
    EXPECT_EQ(scene.getMeshBatch(meshBatchIDs[0]), nullptr);
    EXPECT_EQ(scene.getMeshBatch(meshBatchIDs[1]), nullptr);
    EXPECT_EQ(scene.getInstanceIndices().pData[primitive.index], UINT32_MAX);
    EXPECT_EQ(scene.getActiveMeshBatches().count, 2u);
    const hz::RenderPrimitiveID survivor = scene.findPrimitive(b);
    const uint32_t              survivingInstance = scene.getInstanceIndices().pData[scene.getPrimitive(survivor)->instanceOffset];
    EXPECT_EQ(scene.getInstances().pData[survivingInstance].primitive, survivor);
    const hz::Entity replacement = world.createPersistentEntity(identity);
    ASSERT_TRUE(world.setComponent(replacement, hz::SceneTypes::meshRenderer, &renderer));
    scene.update();
    const hz::RenderPrimitiveID reused = scene.findPrimitive(replacement);
    EXPECT_EQ(reused.index, primitive.index);
    EXPECT_NE(reused.generation, primitive.generation);
    EXPECT_EQ(scene.getActiveMeshBatches().count, 4u);
    for (hz::RenderMeshBatchID old : meshBatchIDs)
    {
        EXPECT_EQ(scene.getMeshBatch(old), nullptr);
        EXPECT_NE(scene.getMeshBatches().pData[old.index].id.generation, old.generation);
    }
}

struct GpuSceneCopy
{
    hz::GPUBuffer hz::GpuSceneBuffers::* table = nullptr;
    uint64_t                             srcOffset = 0;
    uint64_t                             dstOffset = 0;
    uint64_t                             size = 0;
};

// Uploads one GpuScene frame, copies table bytes into the readback buffer and waits for the GPU.
static void renderGpuSceneFrame(hz::RenderContext& context, hz::GpuScene& gpu, const hz::GPUBuffer& readback, hz::Span<GpuSceneCopy> copies)
{
    ASSERT_TRUE(gpu.beginFrame());
    hz::CommandList& commands = context.acquireCommandList();
    gpu.recordUpload(commands);
    for (const GpuSceneCopy& copy : copies)
        commands.copyBuffer(readback, copy.dstOffset, gpu.getBuffers().*copy.table, copy.srcOffset, copy.size);
    const hz::SubmitHandle submission = context.submit(commands);
    gpu.endFrame(submission);
    context.wait(submission);
}

static float translationX(const Matrix4& matrix) { return (float)matrix.getElem(3, 0); }

TEST_F(TypedAssetLiveTest, GpuSceneUploadsChangesAndAdvancesHistoryOnlyForSubmittedFrames)
{
    writeTexture();
    writeScene();
    writeGeometry();
    hz::SceneAsset assets;
    ASSERT_TRUE(assets.parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(assets, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "GpuSceneTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = RD_OTHER_FILES });
    hz::AssetManager  assetManager(catalog, &context);
    loaders.registerLoaders(assetManager);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, assetManager);
    const hz::Entity       a = world.createEntity(), b = world.createEntity();
    const hz::MeshRenderer renderer = { .mesh = assets.getMeshes().pData[0].id };
    hz::LocalTransform     transform = { .translation = Vector3(1, 0, 0) };
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::localTransform, &transform));
    scene.update();
    const hz::AssetHandle meshHandle = assetManager.find(renderer.mesh);
    wait(assetManager, meshHandle);
    scene.update();
    const hz::RenderPrimitiveID primitive = scene.findPrimitive(a);
    const hz::GpuMeshID         meshID = scene.getPrimitive(primitive)->mesh;
    const uint32_t              instanceIndex = scene.getInstanceIndices().pData[scene.getPrimitive(primitive)->instanceOffset];
    hz::RenderMeshBatchID       meshBatchID;
    for (uint32_t index : scene.getActiveMeshBatches())
        if (scene.getMeshBatches().pData[index].primitive == primitive)
            meshBatchID = scene.getMeshBatches().pData[index].id;
    ASSERT_TRUE(meshBatchID.isValid());
    const hz::GpuMaterialID materialID = scene.getMeshBatch(meshBatchID)->material;
    const uint32_t textureSrv = assetManager.getTexture(assetManager.find(assets.getTextures().pData[0].id))->getTexture().getSrvIndex();

    hz::GpuScene              gpu(scene, assetManager, context);
    hz::GPUBuffer             readback = context.createBuffer({ .size = 1024,
                                                                .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                                .startState = RESOURCE_STATE_COPY_DEST,
                                                                .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    const uint8_t*            pBytes = (const uint8_t*)readback.get()->pCpuMappedAddress;
    const hz::RenderInstance& copiedInstance = *(const hz::RenderInstance*)pBytes;
    const hz::GpuMeshRecord&  copiedMesh = *(const hz::GpuMeshRecord*)(pBytes + 256);
    const GpuSceneCopy        instanceCopy = { .table = &hz::GpuSceneBuffers::instances,
                                               .srcOffset = sizeof(hz::RenderInstance) * instanceIndex,
                                               .size = sizeof(hz::RenderInstance) };
    const GpuSceneCopy        meshCopy = { .table = &hz::GpuSceneBuffers::meshes,
                                           .srcOffset = sizeof(hz::GpuMeshRecord) * meshID.index,
                                           .dstOffset = 256,
                                           .size = sizeof(hz::GpuMeshRecord) };
    const GpuSceneCopy        firstCopies[] = {
        instanceCopy,
        meshCopy,
        { .table = &hz::GpuSceneBuffers::materials,
          .srcOffset = sizeof(hz::GpuMaterialRecord) * materialID.index,
          .dstOffset = 512,
          .size = sizeof(hz::GpuMaterialRecord) },
    };
    renderGpuSceneFrame(context, gpu, readback, firstCopies);
    EXPECT_EQ(copiedInstance.id, scene.getInstances().pData[instanceIndex].id);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.current), 1);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.previous), 1);
    EXPECT_EQ(copiedMesh.id, meshID);
    EXPECT_EQ(copiedMesh.indexStride, 2u);
    EXPECT_EQ(copiedMesh.submeshCount, 1u);
    EXPECT_EQ(copiedMesh.indexSrv, assetManager.getMesh(meshHandle)->getIndexBuffer().getSrvIndex());
    const hz::GpuMaterialRecord& copiedMaterial = *(const hz::GpuMaterialRecord*)(pBytes + 512);
    EXPECT_EQ(copiedMaterial.id, materialID);
    EXPECT_FLOAT_EQ(copiedMaterial.roughnessFactor, 0.8f);
    ASSERT_TRUE(copiedMaterial.baseColorTexture.isValid());
    const hz::RenderSceneUploads acknowledged = scene.getPendingUploads();
    EXPECT_EQ(acknowledged.primitives.count + acknowledged.instances.count + acknowledged.meshBatches.count +
                  acknowledged.instanceIndices.count,
              0u);
    EXPECT_FALSE(acknowledged.activePrimitives || acknowledged.activeInstances || acknowledged.activeMeshBatches);

    // Unchanged tables persist on the GPU without another upload.
    transform.translation = Vector3(2, 0, 0);
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::localTransform, &transform));
    const GpuSceneCopy movedCopies[] = {
        instanceCopy,
        { .table = &hz::GpuSceneBuffers::textures,
          .srcOffset = sizeof(hz::GpuTextureRecord) * copiedMaterial.baseColorTexture.index,
          .dstOffset = 768,
          .size = sizeof(hz::GpuTextureRecord) },
    };
    renderGpuSceneFrame(context, gpu, readback, movedCopies);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.current), 2);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.previous), 1);
    const hz::GpuTextureRecord& copiedTexture = *(const hz::GpuTextureRecord*)(pBytes + 768);
    EXPECT_EQ(copiedTexture.id, copiedMaterial.baseColorTexture);
    EXPECT_EQ(copiedTexture.srv, textureSrv);

    // A cancelled frame keeps its changes pending and does not advance history.
    transform.translation = Vector3(3, 0, 0);
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::localTransform, &transform));
    ASSERT_TRUE(gpu.beginFrame());
    gpu.cancelFrame();
    renderGpuSceneFrame(context, gpu, readback, { &instanceCopy, 1 });
    EXPECT_FLOAT_EQ(translationX(copiedInstance.current), 3);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.previous), 2);
    renderGpuSceneFrame(context, gpu, readback, { &instanceCopy, 1 });
    EXPECT_FLOAT_EQ(translationX(copiedInstance.current), 3);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.previous), 3);

    // Changes accumulate across updates until the next submitted frame.
    for (uint32_t i = 0; i < 12; ++i)
    {
        transform.translation = Vector3((float)i + 100, 0, 0);
        ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::localTransform, &transform));
        scene.update();
    }
    renderGpuSceneFrame(context, gpu, readback, { &instanceCopy, 1 });
    EXPECT_FLOAT_EQ(translationX(copiedInstance.current), 111);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.previous), 3);

    const MemoryTrackingStats before = memGetTrackingStats();
    for (uint32_t i = 0; i < 30; ++i)
    {
        transform.translation = Vector3((float)i + 200, 0, 0);
        ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::localTransform, &transform));
        renderGpuSceneFrame(context, gpu, readback, { &instanceCopy, 1 });
        EXPECT_FLOAT_EQ(translationX(copiedInstance.current), (float)i + 200);
        EXPECT_FLOAT_EQ(translationX(copiedInstance.previous), i ? (float)i + 199 : 111.0f);
    }
    const MemoryTrackingStats after = memGetTrackingStats();
    if (before.trackingEnabled)
    {
        EXPECT_EQ(before.totalAllocationCount, after.totalAllocationCount);
        EXPECT_EQ(before.reallocationCount, after.reallocationCount);
    }

    // A teleport resets history for the next submitted frame.
    transform.translation = Vector3(300, 0, 0);
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::localTransform, &transform));
    scene.resetHistory(a);
    renderGpuSceneFrame(context, gpu, readback, { &instanceCopy, 1 });
    EXPECT_FLOAT_EQ(translationX(copiedInstance.current), 300);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.previous), 300);

    // Removal frees the slots; re-adding reuses them with new generations and fresh history.
    ASSERT_TRUE(world.removeComponent(a, hz::SceneTypes::meshRenderer));
    const GpuSceneCopy activeCopies[] = {
        { .table = &hz::GpuSceneBuffers::activePrimitives, .size = sizeof(uint32_t) },
        { .table = &hz::GpuSceneBuffers::instanceIndices,
          .srcOffset = sizeof(uint32_t) * primitive.index,
          .dstOffset = sizeof(uint32_t),
          .size = sizeof(uint32_t) },
    };
    renderGpuSceneFrame(context, gpu, readback, activeCopies);
    EXPECT_EQ(scene.getActivePrimitives().count, 1u);
    EXPECT_EQ(((const uint32_t*)pBytes)[0], scene.findPrimitive(b).index);
    EXPECT_EQ(((const uint32_t*)pBytes)[1], UINT32_MAX);
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    renderGpuSceneFrame(context, gpu, readback, { &instanceCopy, 1 });
    const hz::RenderPrimitiveID reused = scene.findPrimitive(a);
    EXPECT_EQ(reused.index, primitive.index);
    EXPECT_NE(reused.generation, primitive.generation);
    ASSERT_EQ(scene.getInstanceIndices().pData[reused.index], instanceIndex);
    EXPECT_EQ(copiedInstance.id, scene.getInstances().pData[instanceIndex].id);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.current), 300);
    EXPECT_FLOAT_EQ(translationX(copiedInstance.previous), 300);

    // Released meshes leave the uploaded tables and retire once the GPU no longer uses them.
    ASSERT_TRUE(world.removeComponent(a, hz::SceneTypes::meshRenderer));
    ASSERT_TRUE(world.removeComponent(b, hz::SceneTypes::meshRenderer));
    renderGpuSceneFrame(context, gpu, readback, { &meshCopy, 1 });
    EXPECT_EQ(scene.getActivePrimitives().count, 0u);
    EXPECT_FALSE(copiedMesh.id.isValid());
    assetManager.update();
    EXPECT_EQ(assetManager.getCount(), 0u);

    // Reloading uses a new slot generation; the old GPU mesh ID stays invalid.
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    scene.update();
    const hz::AssetHandle reloaded = assetManager.find(renderer.mesh);
    wait(assetManager, reloaded);
    renderGpuSceneFrame(context, gpu, readback, { &meshCopy, 1 });
    EXPECT_NE(scene.getPrimitive(scene.findPrimitive(a))->mesh, meshID);
    EXPECT_NE(copiedMesh.id, meshID);
    EXPECT_EQ(scene.getActiveMeshBatches().count, 1u);
}

TEST_F(TypedAssetLiveTest, GpuSceneUploadsLightsAndCameraHistoryAndRetiresEmissionTextures)
{
    writeTexture();
    writeScene();
    writeGeometry();
    hz::SceneAsset assets;
    ASSERT_TRUE(assets.parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(assets, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "GpuSceneLightTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = RD_OTHER_FILES });
    hz::AssetManager  assetManager(catalog, &context);
    loaders.registerLoaders(assetManager);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, assetManager);
    const hz::Entity       entity = world.createEntity();
    hz::Light              light = { .type = hz::LightType::Mesh, .intensity = 4, .texture = assets.getTextures().pData[0].id };
    hz::Camera             lens = { .verticalFov = 0.8f };
    hz::LocalTransform     transform = { .translation = Vector3(1, 0, 0) };
    const hz::MeshRenderer renderer = { .mesh = assets.getMeshes().pData[0].id };
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::light, &light));
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::camera, &lens));
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
    scene.update();
    const hz::RenderLightID  lightID = scene.findLight(entity);
    const hz::RenderCameraID cameraID = scene.findCamera(entity);
    const hz::AssetHandle    textureHandle = assetManager.find(light.texture);
    EXPECT_EQ(scene.getLight(lightID)->ready, 0u);
    wait(assetManager, textureHandle);
    scene.update();
    EXPECT_EQ(scene.getLight(lightID)->ready, 0u); // Mesh emitter still has no mesh.
    EXPECT_TRUE(scene.getLight(lightID)->texture.isValid());
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::meshRenderer, &renderer));
    scene.update();
    wait(assetManager, assetManager.find(renderer.mesh));
    scene.update();
    ASSERT_EQ(scene.getLight(lightID)->ready, 1u);
    EXPECT_EQ(scene.getLight(lightID)->primitive, scene.findPrimitive(entity));

    hz::GpuScene gpu(scene, assetManager, context);
    gpu.reserve(1, 1, 1, 1);
    hz::GPUBuffer           readback = context.createBuffer({ .size = 1024,
                                                              .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                              .startState = RESOURCE_STATE_COPY_DEST,
                                                              .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    const uint8_t*          pBytes = (const uint8_t*)readback.get()->pCpuMappedAddress;
    const hz::RenderCamera& copiedCamera = *(const hz::RenderCamera*)pBytes;
    const hz::RenderLight&  copiedLight = *(const hz::RenderLight*)(pBytes + 512);
    const uint32_t*         pActive = (const uint32_t*)(pBytes + 1000);
    static_assert(sizeof(hz::RenderCamera) <= 512 && sizeof(hz::RenderLight) <= 256);
    const GpuSceneCopy copies[] = {
        { .table = &hz::GpuSceneBuffers::cameras,
          .srcOffset = sizeof(hz::RenderCamera) * cameraID.index,
          .size = sizeof(hz::RenderCamera) },
        { .table = &hz::GpuSceneBuffers::lights,
          .srcOffset = sizeof(hz::RenderLight) * lightID.index,
          .dstOffset = 512,
          .size = sizeof(hz::RenderLight) },
        { .table = &hz::GpuSceneBuffers::activeLights, .dstOffset = 1000, .size = sizeof(uint32_t) },
        { .table = &hz::GpuSceneBuffers::activeCameras, .dstOffset = 1004, .size = sizeof(uint32_t) },
    };
    renderGpuSceneFrame(context, gpu, readback, copies);
    EXPECT_EQ(copiedCamera.id, cameraID);
    EXPECT_FLOAT_EQ(translationX(copiedCamera.current), 1);
    EXPECT_FLOAT_EQ(translationX(copiedCamera.previous), 1);
    EXPECT_FLOAT_EQ(copiedCamera.lens.verticalFov, 0.8f);
    EXPECT_EQ(copiedLight.id, lightID);
    EXPECT_FLOAT_EQ(copiedLight.intensity, 4);
    EXPECT_EQ(copiedLight.ready, 1u);
    EXPECT_EQ(copiedLight.texture, scene.getLight(lightID)->texture);
    EXPECT_EQ(pActive[0], lightID.index);
    EXPECT_EQ(pActive[1], cameraID.index);

    transform.translation = Vector3(2, 0, 0);
    lens.verticalFov = 0.6f;
    light.intensity = 8;
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::camera, &lens));
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::light, &light));
    renderGpuSceneFrame(context, gpu, readback, copies);
    EXPECT_FLOAT_EQ(translationX(copiedCamera.current), 2);
    EXPECT_FLOAT_EQ(translationX(copiedCamera.previous), 1);
    EXPECT_FLOAT_EQ(copiedCamera.lens.verticalFov, 0.6f);
    EXPECT_FLOAT_EQ(copiedCamera.previousLens.verticalFov, 0.8f);
    EXPECT_FLOAT_EQ(copiedLight.intensity, 8);
    EXPECT_FLOAT_EQ(translationX(copiedLight.transform), 2);
    renderGpuSceneFrame(context, gpu, readback, copies);
    EXPECT_FLOAT_EQ(translationX(copiedCamera.previous), 2);
    EXPECT_FLOAT_EQ(copiedCamera.previousLens.verticalFov, 0.6f);

    const MemoryTrackingStats before = memGetTrackingStats();
    for (uint32_t i = 0; i < 30; ++i)
    {
        transform.translation = Vector3((float)i + 4, 0, 0);
        lens.verticalFov = 0.6f + 0.001f * i;
        ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
        ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::camera, &lens));
        renderGpuSceneFrame(context, gpu, readback, copies);
        EXPECT_FLOAT_EQ(translationX(copiedCamera.current), (float)i + 4);
        EXPECT_FLOAT_EQ(translationX(copiedLight.transform), (float)i + 4);
        EXPECT_FLOAT_EQ(translationX(copiedCamera.previous), i ? (float)i + 3 : 2.0f);
    }
    const MemoryTrackingStats after = memGetTrackingStats();
    if (before.trackingEnabled)
    {
        EXPECT_EQ(before.totalAllocationCount, after.totalAllocationCount);
        EXPECT_EQ(before.reallocationCount, after.reallocationCount);
    }

    // A camera cut resets history for the next submitted frame.
    transform.translation = Vector3(100, 0, 0);
    lens.verticalFov = 1;
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::camera, &lens));
    scene.resetHistory(entity);
    renderGpuSceneFrame(context, gpu, readback, copies);
    EXPECT_FLOAT_EQ(translationX(copiedCamera.previous), 100);
    EXPECT_FLOAT_EQ(copiedCamera.previousLens.verticalFov, 1);

    // Removing only the mesh disables its emitter, keeping the light slot and texture lease.
    ASSERT_TRUE(world.removeComponent(entity, hz::SceneTypes::meshRenderer));
    renderGpuSceneFrame(context, gpu, readback, copies);
    assetManager.update();
    EXPECT_EQ(scene.findLight(entity), lightID);
    EXPECT_EQ(copiedLight.ready, 0u);
    EXPECT_FALSE(scene.getLight(lightID)->primitive.isValid());
    EXPECT_EQ(assetManager.getState(textureHandle), hz::AssetState::Ready);

    light.type = hz::LightType::Dome;
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::light, &light));
    renderGpuSceneFrame(context, gpu, readback, copies);
    const hz::GpuTextureID textureID = copiedLight.texture;
    ASSERT_TRUE(textureID.isValid());
    ASSERT_TRUE(world.removeComponent(entity, hz::SceneTypes::light));
    ASSERT_TRUE(world.removeComponent(entity, hz::SceneTypes::camera));
    const GpuSceneCopy textureCopy = { .table = &hz::GpuSceneBuffers::textures,
                                       .srcOffset = sizeof(hz::GpuTextureRecord) * textureID.index,
                                       .dstOffset = 768,
                                       .size = sizeof(hz::GpuTextureRecord) };
    renderGpuSceneFrame(context, gpu, readback, { &textureCopy, 1 });
    EXPECT_EQ(scene.getActiveLights().count, 0u);
    EXPECT_EQ(scene.getActiveCameras().count, 0u);
    EXPECT_FALSE(((const hz::GpuTextureRecord*)(pBytes + 768))->id.isValid());
    assetManager.update();
    EXPECT_EQ(assetManager.getCount(), 0u);

    light.texture = {};
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::light, &light));
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::camera, &lens));
    renderGpuSceneFrame(context, gpu, readback, copies);
    const hz::RenderCameraID reusedCamera = scene.findCamera(entity);
    EXPECT_EQ(reusedCamera.index, cameraID.index);
    EXPECT_NE(reusedCamera.generation, cameraID.generation);
    EXPECT_EQ(scene.findLight(entity).index, lightID.index);
    EXPECT_NE(scene.findLight(entity).generation, lightID.generation);
    EXPECT_EQ(copiedCamera.id, reusedCamera);
    EXPECT_FLOAT_EQ(translationX(copiedCamera.previous), 100);
}

TEST_F(AssetManagerTest, GpuSceneUploadsSparseChangesForOneHundredThousandEntities)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(
        catalog.add({ .id = assetID(1), .type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::RenderContext context({ .pAppName = "GpuSceneStressTest" });
    hz::AssetManager  assetManager(catalog, &context);
    assetManager.setLoader(hz::AssetType::Mesh, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, assetManager);
    constexpr uint32_t     kEntityCount = 100000;
    constexpr uint32_t     kStride = 100;
    const hz::MeshRenderer renderer = { .mesh = assetID(1) };
    hz::Array<hz::Entity>  entities;
    entities.reserve(kEntityCount);
    for (uint32_t i = 0; i < kEntityCount; ++i)
    {
        const hz::Entity         entity = world.createEntity();
        const hz::LocalTransform transform = { .translation = Vector3((float)i, 0, 0) };
        ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::meshRenderer, &renderer));
        ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
        entities.pushBack(entity);
    }
    hz::GpuScene gpu(scene, assetManager, context);
    gpu.reserve(kEntityCount, kEntityCount);
    hz::GPUBuffer             readback = context.createBuffer({ .size = sizeof(hz::RenderInstance),
                                                                .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                                .startState = RESOURCE_STATE_COPY_DEST,
                                                                .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    const hz::RenderInstance& copied = *(const hz::RenderInstance*)readback.get()->pCpuMappedAddress;
    const int64_t             fullStart = getUSec(true);
    renderGpuSceneFrame(context, gpu, readback, {});
    const int64_t fullUSec = getUSec(true) - fullStart;

    // Move every hundredth entity per frame: 1000 separate instance ranges.
    int64_t sparseUSec = 0;
    for (uint32_t frame = 0; frame < 10; ++frame)
    {
        for (uint32_t i = frame; i < kEntityCount; i += kStride)
        {
            const hz::LocalTransform transform = { .translation = Vector3((float)i, (float)frame + 1, 0) };
            ASSERT_TRUE(world.setComponent(entities[i], hz::SceneTypes::localTransform, &transform));
        }
        // Primitive and instance slots are allocated together, so their indices match.
        const uint32_t     instance = scene.findPrimitive(entities[frame]).index;
        const GpuSceneCopy copy = { .table = &hz::GpuSceneBuffers::instances,
                                    .srcOffset = sizeof(hz::RenderInstance) * instance,
                                    .size = sizeof(hz::RenderInstance) };
        const int64_t      start = getUSec(true);
        renderGpuSceneFrame(context, gpu, readback, { &copy, 1 });
        sparseUSec += getUSec(true) - start;
        ASSERT_EQ(scene.getInstances().pData[instance].primitive, scene.findPrimitive(entities[frame]));
        EXPECT_FLOAT_EQ((float)copied.current.getElem(3, 1), (float)frame + 1);
        EXPECT_EQ(scene.getChanges().instances.ranges.count, kEntityCount / kStride);
    }
    // Telemetry only; timings vary across machines.
    printf("GpuScene %u entities: full upload frame %.2f ms, 1%% sparse frame %.2f ms\n", kEntityCount, fullUSec / 1000.0,
           sparseUSec / 10000.0);
}

TEST_F(TypedAssetLiveTest, InstantiatesEditsSavesAndReloadsWorldWithGpuAssetRebinding)
{
    writeTexture();
    writeScene();
    writeGeometry();
    hz::SceneAsset assets;
    ASSERT_TRUE(assets.parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(assets, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "WorldWorkflowTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = RD_OTHER_FILES });
    hz::AssetManager  assetManager(catalog, &context);
    loaders.registerLoaders(assetManager);
    const hz::AssetHandle source = assetManager.request(assets.getID());
    wait(assetManager, source);
    ASSERT_EQ(assetManager.getState(source), hz::AssetState::Ready);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::unique_ptr<hz::World> world = hz::make_unique<hz::World>(types);
    const hz::Entity          root = assetManager.getScene(source)->instantiate(*world);
    hz::Entity                first, second;
    {
        hz::WorldQuery query(*world, { hz::SceneTypes::meshRenderer });
        struct Nodes
        {
            hz::Entity* first;
            hz::Entity* second;
            hz::AssetID mesh;
        } nodes = { &first, &second, assets.getMeshes().pData[0].id };
        query.each(
            [](hz::Entity entity, hz::Span<const void*> fields, void* user)
            {
                Nodes& nodes = *(Nodes*)user;
                *(((const hz::MeshRenderer*)fields.pData[0])->mesh == nodes.mesh ? nodes.first : nodes.second) = entity;
                return true;
            },
            &nodes);
    }
    ASSERT_TRUE(first.isValid() && second.isValid());
    hz::LocalMatrix transform = { .value = Matrix4::translation(Vector3(5, 0, 0)) };
    transform.value.setElem(1, 0, 0.25f);
    ASSERT_TRUE(world->setComponent(root, hz::SceneTypes::localMatrix, &transform));
    ASSERT_TRUE(world->setParent(first, second, false));
    const hz::Name name = { "Edited" };
    ASSERT_TRUE(world->setComponent(first, hz::SceneTypes::name, &name));
    hz::MeshRenderer renderer = { .mesh = assets.getMeshes().pData[0].id };
    renderer.overrides.pushBack(
        { .submesh = assets.getMeshes().pData[0].submeshes.pData[0].id, .material = assets.getMaterials().pData[0].id });
    ASSERT_TRUE(world->setComponent(first, hz::SceneTypes::meshRenderer, &renderer));
    const hz::Guid   rootID = world->getEntityGuid(root), firstID = world->getEntityGuid(first), secondID = world->getEntityGuid(second);
    hz::WorldArchive saved;
    ASSERT_TRUE(saved.capture(*world));
    ASSERT_TRUE(saved.save(RD_OTHER_FILES, "edited.world.json"));
    hz::Array<char> original;
    ASSERT_TRUE(saved.serialize(original));
    hz::unique_ptr<hz::RenderScene> scene = hz::make_unique<hz::RenderScene>(*world, assetManager);
    scene->update();
    ASSERT_EQ(scene->getActiveMeshBatches().count, 2u);
    ASSERT_TRUE(assetManager.release(source));
    hz::unique_ptr<hz::GpuScene> gpu = hz::make_unique<hz::GpuScene>(*scene, assetManager, context);
    hz::GPUBuffer                readback = context.createBuffer({ .size = sizeof(hz::RenderInstance) + sizeof(hz::GpuMaterialRecord) + 36,
                                                                   .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                                   .startState = RESOURCE_STATE_COPY_DEST,
                                                                   .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    const hz::AssetHandle        oldMesh = assetManager.find(renderer.mesh);
    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        const hz::Entity entity = world->getEntity(firstID);
        ASSERT_TRUE(entity.isValid());
        EXPECT_EQ(world->getParent(entity), world->getEntity(secondID));
        EXPECT_EQ(world->getParent(world->getEntity(secondID)), world->getEntity(rootID));
        EXPECT_STREQ(((const hz::Name*)world->getComponent(entity, hz::SceneTypes::name))->value, "Edited");
        const hz::MeshRenderer* meshRenderer = (const hz::MeshRenderer*)world->getComponent(entity, hz::SceneTypes::meshRenderer);
        ASSERT_NE(meshRenderer, nullptr);
        ASSERT_EQ(meshRenderer->overrides.size(), 1u);
        EXPECT_EQ(meshRenderer->mesh, renderer.mesh);
        EXPECT_EQ(meshRenderer->overrides[0].submesh, renderer.overrides[0].submesh);
        EXPECT_EQ(meshRenderer->overrides[0].material, renderer.overrides[0].material);
        ASSERT_TRUE(gpu->beginFrame());
        ASSERT_EQ(scene->getActiveMeshBatches().count, 2u);
        const hz::RenderPrimitiveID primitiveID = scene->findPrimitive(entity);
        const hz::RenderPrimitive&  primitive = *scene->getPrimitive(primitiveID);
        const uint32_t              instance = scene->getInstanceIndices().pData[primitive.instanceOffset];
        const hz::RenderMeshBatch*  meshBatch = nullptr;
        for (uint32_t index : scene->getActiveMeshBatches())
            if (scene->getMeshBatches().pData[index].primitive == primitiveID)
                meshBatch = &scene->getMeshBatches().pData[index];
        ASSERT_NE(meshBatch, nullptr);
        EXPECT_EQ(meshBatch->submesh, renderer.overrides[0].submesh);
        const hz::GpuMaterialRecord* material = gpu->getAssetTables().getMaterial(meshBatch->material);
        ASSERT_NE(material, nullptr);
        EXPECT_NE(gpu->getAssetTables().getTexture(material->baseColorTexture), nullptr);
        const hz::MeshAsset* mesh = assetManager.getMesh(assetManager.find(renderer.mesh));
        ASSERT_NE(mesh, nullptr);
        hz::CommandList& commands = context.acquireCommandList();
        gpu->recordUpload(commands);
        commands.copyBuffer(readback, 0, gpu->getBuffers().instances, instance * sizeof(hz::RenderInstance), sizeof(hz::RenderInstance));
        commands.copyBuffer(readback, sizeof(hz::RenderInstance), gpu->getBuffers().materials,
                            meshBatch->material.index * sizeof(hz::GpuMaterialRecord), sizeof(hz::GpuMaterialRecord));
        commands.copyBuffer(readback, sizeof(hz::RenderInstance) + sizeof(hz::GpuMaterialRecord), mesh->getVertexBuffer(0), 0, 36);
        const hz::SubmitHandle submission = context.submit(commands);
        gpu->endFrame(submission);
        // Destroying the GPU scene, bindings and World does not wait; buffers and payloads retire after the frame.
        gpu = nullptr;
        scene = nullptr;
        world = nullptr;
        context.wait(submission);
        assetManager.update();
        EXPECT_EQ(assetManager.getCount(), 0u);
        EXPECT_EQ(assetManager.getMesh(oldMesh), nullptr);
        const hz::RenderInstance* copied = (const hz::RenderInstance*)readback.get()->pCpuMappedAddress;
        EXPECT_EQ(copied->entityGuid, firstID);
        EXPECT_FLOAT_EQ((float)copied->current.getElem(3, 0), 6);
        EXPECT_FLOAT_EQ((float)copied->previous.getElem(3, 0), 6);
        EXPECT_FLOAT_EQ((float)copied->current.getElem(1, 0), 0.25f);
        const hz::GpuMaterialRecord* copiedMaterial = (const hz::GpuMaterialRecord*)((const uint8_t*)copied + sizeof(*copied));
        EXPECT_FLOAT_EQ(copiedMaterial->roughnessFactor, 0.8f);
        EXPECT_FLOAT_EQ(copiedMaterial->baseColorFactor[1], 0.5f);
        const float* vertices = (const float*)((const uint8_t*)copiedMaterial + sizeof(*copiedMaterial));
        EXPECT_FLOAT_EQ(vertices[0], -1);
        EXPECT_FLOAT_EQ(vertices[4], -5);
        if (pass == 1)
            break;
        hz::WorldArchive loaded;
        ASSERT_TRUE(loaded.load(RD_OTHER_FILES, "edited.world.json"));
        ASSERT_TRUE(loaded.instantiate(types, world));
        EXPECT_EQ(loaded.getID(), saved.getID());
        ASSERT_TRUE(loaded.capture(*world));
        hz::Array<char> restored;
        ASSERT_TRUE(loaded.serialize(restored));
        ASSERT_EQ(restored.size(), original.size());
        EXPECT_EQ(memcmp(restored.data(), original.data(), original.size()), 0);
        scene = hz::make_unique<hz::RenderScene>(*world, assetManager);
        scene->update();
        const hz::AssetHandle reloadedMesh = assetManager.find(renderer.mesh);
        ASSERT_TRUE(reloadedMesh.isValid());
        EXPECT_NE(reloadedMesh, oldMesh);
        wait(assetManager, reloadedMesh);
        wait(assetManager, assetManager.find(assets.getMeshes().pData[1].id));
        scene->update();
        gpu = hz::make_unique<hz::GpuScene>(*scene, assetManager, context);
    }
    assetManager.shutdown();
}

TEST_F(TypedAssetLiveTest, RejectsBrokenGeometryAndMismatchedSubassetsAndCancelsLoads)
{
    writeTexture();
    writeScene();
    writeGeometry();
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(scene, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "TypedAssetFailureTest" });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = RD_OTHER_FILES });

    // Every prefix must fail without leaking partially decoded blocks or publishing GPU buffers.
    const uintmax_t size = std::filesystem::file_size(directory / "scene.bin");
    const uintmax_t prefixes[] = { 0, 10, 14, 14 + sizeof(Geometry), size - 1 };
    for (uintmax_t prefix : prefixes)
    {
        writeGeometry();
        std::filesystem::resize_file(directory / "scene.bin", prefix);
        hz::AssetManager assetManager(catalog);
        loaders.registerLoaders(assetManager);
        const hz::AssetHandle mesh = assetManager.request(scene.getMeshes().pData[0].id);
        wait(assetManager, mesh);
        EXPECT_EQ(assetManager.getError(mesh), hz::AssetError::UploadFailed) << prefix;
        EXPECT_EQ(assetManager.getMesh(mesh), nullptr);
    }
    writeGeometry((IndexType)2);
    {
        hz::AssetManager assetManager(catalog);
        loaders.registerLoaders(assetManager);
        const hz::AssetHandle mesh = assetManager.request(scene.getMeshes().pData[0].id);
        wait(assetManager, mesh);
        EXPECT_EQ(assetManager.getError(mesh), hz::AssetError::UploadFailed);
        EXPECT_EQ(assetManager.getMesh(mesh), nullptr);
    }
    std::filesystem::remove(directory / "scene.bin");
    {
        hz::AssetManager assetManager(catalog);
        loaders.registerLoaders(assetManager);
        const hz::AssetHandle mesh = assetManager.request(scene.getMeshes().pData[0].id);
        wait(assetManager, mesh);
        EXPECT_EQ(assetManager.getError(mesh), hz::AssetError::UploadFailed);
    }
    writeGeometry();
    hz::AssetCatalogEntry wrong = *catalog.find(scene.getMeshes().pData[0].id);
    wrong.location.elementIndex = 1;
    ASSERT_TRUE(catalog.replace(wrong));
    {
        hz::AssetManager assetManager(catalog);
        loaders.registerLoaders(assetManager);
        const hz::AssetHandle mismatched = assetManager.request(wrong.id);
        const hz::AssetHandle root = assetManager.request(scene.getID());
        wait(assetManager, root);
        EXPECT_EQ(assetManager.getError(root), hz::AssetError::DependencyFailed);
        EXPECT_EQ(assetManager.getError(mismatched), hz::AssetError::LoadFailed);
    }
    wrong.location.elementIndex = 0;
    ASSERT_TRUE(catalog.replace(wrong));
    for (uint32_t phase = 0; phase < 2; ++phase)
    {
        hz::AssetManager assetManager(catalog);
        loaders.registerLoaders(assetManager);
        const hz::AssetHandle root = assetManager.request(scene.getID());
        if (phase)
        {
            for (uint32_t i = 0; i < 5000; ++i)
            {
                assetManager.update();
                if (assetManager.getState(assetManager.find(wrong.id)) == hz::AssetState::UploadingGPU)
                    break;
                threadSleep(1);
            }
            EXPECT_EQ(assetManager.getState(assetManager.find(wrong.id)), hz::AssetState::UploadingGPU);
        }
        else
            assetManager.update();
        assetManager.release(root);
        assetManager.shutdown();
        EXPECT_EQ(assetManager.getCount(), 0u);
    }
}

TEST_F(TypedAssetLiveTest, IndirectDrawIdsSelectDistinctGeometryAndReadThreeUint16Indices)
{
    writeTexture();
    writeScene();
    writeGeometry(INDEX_TYPE_UINT16);
    hz::SceneAsset asset;
    ASSERT_TRUE(asset.parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(asset, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "IndirectDrawIdTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = RD_OTHER_FILES });
    hz::AssetManager  assetManager(catalog, &context);
    loaders.registerLoaders(assetManager);
    const hz::AssetHandle handle = assetManager.request(asset.getMeshes().pData[0].id);
    wait(assetManager, handle);
    ASSERT_EQ(assetManager.getState(handle), hz::AssetState::Ready);
    const hz::MeshAsset* mesh = assetManager.getMesh(handle);
    ASSERT_NE(mesh, nullptr);
    ASSERT_EQ(mesh->getIndexType(), INDEX_TYPE_UINT16);
    // Three indices consume six bytes; RAW loads require a second complete DWORD.
    ASSERT_EQ(mesh->getIndexBuffer().getSize(), 8u);
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, shaders.string().c_str());
    hz::GPUPipeline pipeline = context.createGraphicsPipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSIndirect" },
                                    { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                        .sourceDirectory = RD_OTHER_FILES,
                        .pFileName = "Geometry.slang" },
        .colorTargets = { { .format = hz::Format::R8G8B8A8_UNORM },
                          { .format = hz::Format::R8G8B8A8_UNORM },
                          { .format = hz::Format::R8G8B8A8_UNORM },
                          { .format = hz::Format::R8G8B8A8_UNORM } },
        .indirectDrawId = true,
    });
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, directory.string().c_str());
    ASSERT_TRUE(pipeline.isValid());
    struct FrameData
    {
        Matrix4 viewProjection = Matrix4::identity();
        Matrix4 previousViewProjection = Matrix4::identity();
        Matrix4 inverseViewProjection = Matrix4::identity();
        Vector4 eye = Vector4(0, 0, -2, 1);
    } frameData;
    hz::RenderInstance instances[2];
    instances[0].current = instances[0].previous = Matrix4::translation(Vector3(-0.5f, 0, 0));
    instances[1].current = instances[1].previous = Matrix4::translation(Vector3(0.5f, 0, 0));
    hz::GpuMaterialRecord materials[2];
    materials[0].baseColorFactor[1] = materials[0].baseColorFactor[2] = 0;
    materials[1].baseColorFactor[0] = materials[1].baseColorFactor[2] = 0;
    materials[0].metallicFactor = materials[1].metallicFactor = 0;
    const float    positions[] = { -0.3f, -0.5f, 0.5f, 0.3f, -0.5f, 0.5f, 0, 0.5f, 0.5f };
    const uint32_t normals[] = { 0x80008000, 0x80008000, 0x80008000 }, uvs[] = { 0, 0, 0 };
    const auto     input = [&](const void* data, uint32_t size, uint32_t stride = 0)
    {
        return context.createBuffer({ .size = size,
                                      .elementCount = stride ? size / stride : size / 4,
                                      .structStride = stride,
                                      .pInitialData = data,
                                      .initialDataSize = size,
                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                      .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                      .descriptors = stride ? DESCRIPTOR_TYPE_BUFFER : DESCRIPTOR_TYPE_BUFFER_RAW });
    };
    hz::GPUBuffer frame = input(&frameData, sizeof(frameData), sizeof(frameData));
    hz::GPUBuffer instanceBuffer = input(instances, sizeof(instances)), materialBuffer = input(materials, sizeof(materials));
    hz::GPUBuffer vertices = input(positions, sizeof(positions)), normal = input(normals, sizeof(normals)), uv = input(uvs, sizeof(uvs));
    struct DrawRecord
    {
        uint32_t instance, material, materialId, alphaMode, positions, normals, texcoords, indices;
        uint32_t indexStride, firstIndex;
        int32_t  vertexOffset;
        uint32_t indexCount;
    };
    DrawRecord records[2] = { { .instance = 0,
                                .material = 0,
                                .materialId = 17,
                                .alphaMode = 0,
                                .positions = vertices.getSrvIndex(),
                                .normals = normal.getSrvIndex(),
                                .texcoords = uv.getSrvIndex(),
                                .indices = mesh->getIndexBuffer().getSrvIndex(),
                                .indexStride = 2,
                                .firstIndex = 0,
                                .vertexOffset = 0,
                                .indexCount = 3 } };
    records[1] = records[0];
    records[1].instance = records[1].material = 1;
    records[1].materialId = 39;
    hz::GPUBuffer draws = input(records, sizeof(records));
    struct DrawCommand
    {
        uint32_t drawId, vertexCount, instanceCount, firstVertex, firstInstance;
    };
    static_assert(sizeof(DrawCommand) == 20);
    const DrawCommand arguments[] = { { 0, 3, 1, 0, 0 }, { 1, 3, 1, 0, 0 } };
    hz::GPUBuffer     indirect = context.createBuffer({ .size = sizeof(arguments),
                                                        .pInitialData = arguments,
                                                        .initialDataSize = sizeof(arguments),
                                                        .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                        .startState = RESOURCE_STATE_INDIRECT_ARGUMENT,
                                                        .descriptors = DESCRIPTOR_TYPE_INDIRECT_BUFFER });
    hz::GPUSampler    sampler = context.createSampler();
    hz::GPUTexture    targets[4];
    for (hz::GPUTexture& target : targets)
    {
        target = context.createTexture({ .width = 64,
                                         .height = 64,
                                         .format = hz::Format::R8G8B8A8_UNORM,
                                         .startState = RESOURCE_STATE_RENDER_TARGET,
                                         .descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                         .renderTarget = true });
    }
    const char      readPixel[] = R"hlsl(
        cbuffer RootConstant0 { uint textureIndex; uint outputIndex; };
        [numthreads(1,1,1)] void CSMain() {
            Texture2D<float4> source = ResourceDescriptorHeap[textureIndex];
            RWStructuredBuffer<uint4> output = ResourceDescriptorHeap[outputIndex];
            output[0] = uint4(round(source.Load(int3(16,32,0)) * 255));
            output[1] = uint4(round(source.Load(int3(48,32,0)) * 255));
        }
    )hlsl";
    hz::GPUPipeline reader = context.createComputePipeline(
        { .shaderDesc = {
              .stages = {
                  { .stage = SHADER_STAGE_COMP, .pSource = readPixel, .sourceSize = sizeof(readPixel) - 1, .pEntryPoint = "CSMain" } } } });
    hz::GPUBuffer    output = context.createBuffer({ .size = 32,
                                                     .elementCount = 2,
                                                     .structStride = 16,
                                                     .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                     .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                     .descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer    readback = context.createBuffer({ .size = 32,
                                                       .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                       .startState = RESOURCE_STATE_COPY_DEST,
                                                       .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::CommandList& commands = context.acquireCommandList();
    commands.beginRendering({ .colorAttachments = {
        { .pTexture = &targets[0], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
        { .pTexture = &targets[1], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
        { .pTexture = &targets[2], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
        { .pTexture = &targets[3], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
    } }, { .buffers = { &frame, &instanceBuffer, &materialBuffer, &vertices, &normal, &uv, &draws, &mesh->getIndexBuffer() } });
    commands.setViewport(0, 0, 64, 64);
    commands.setScissor(0, 0, 64, 64);
    commands.setPipeline(pipeline);
    uint32_t constants[27] = {};
    constants[3] = frame.getSrvIndex();
    constants[4] = instanceBuffer.getSrvIndex();
    constants[5] = materialBuffer.getSrvIndex();
    constants[6] = UINT32_MAX;
    constants[7] = sampler.getIndex();
    constants[11] = draws.getSrvIndex();
    commands.setRootConstant({ constants, 27 });
    commands.drawIndirect(indirect, 0, 2);
    commands.endRendering();
    commands.setPipeline(reader);
    commands.setRootConstant({ targets[2].getSrvIndex(), output.getUavIndex() });
    commands.dispatch(1, 1, 1, { .sampledTextures = { &targets[2] }, .storageBuffers = { &output } });
    commands.copyBuffer(readback, 0, output, 0, 32);
    context.wait(context.submit(commands));
    const uint32_t* pixels = (const uint32_t*)readback.get()->pCpuMappedAddress;
    ASSERT_NE(pixels, nullptr);
    EXPECT_EQ(pixels[0], 255u);
    EXPECT_EQ(pixels[1], 0u);
    EXPECT_EQ(pixels[2], 0u);
    EXPECT_EQ(pixels[4], 0u);
    EXPECT_EQ(pixels[5], 255u);
    EXPECT_EQ(pixels[6], 0u);
}

TEST_F(SceneAssetLoaderTest, TracesDxrHitsAndMissesAndUpdatesTopLevelInstances)
{
    hz::RenderContext context({ .pAppName = "DxrPipelineTest", .enableGpuValidation = true });
    if (!context.supportsRayTracing())
    {
        GTEST_SKIP() << "Selected device does not support DXR pipelines and inline ray queries";
    }
    const char source[] = R"slang(
        cbuffer RootConstant0 { uint topIndex; uint outputIndex; };
        struct Payload { uint value; };
        [shader("raygeneration")] void RayGeneration() {
            RaytracingAccelerationStructure scene = ResourceDescriptorHeap[topIndex];
            RWStructuredBuffer<uint> output = ResourceDescriptorHeap[outputIndex];
            RayDesc ray;
            ray.Origin = float3(DispatchRaysIndex().x * 4.0, 0, -1);
            ray.Direction = float3(0, 0, 1); ray.TMin = 0.001; ray.TMax = 10;
            Payload payload = { 0 };
            TraceRay(scene, RAY_FLAG_NONE, 0xff, 0, 0, 0, ray, payload);
            output[DispatchRaysIndex().x] = payload.value;
        }
        [shader("miss")] void Miss(inout Payload payload) { payload.value = 7; }
        [shader("closesthit")] void ClosestHit(inout Payload payload, BuiltInTriangleIntersectionAttributes attributes) {
            payload.value = 100 + InstanceID() + PrimitiveIndex();
        }
    )slang";
    FileStream stream = {};
    ASSERT_TRUE(fsOpenStreamFromPath(RD_OTHER_FILES, "dxr-regression.slang", FM_WRITE, &stream));
    const size_t written = fsWriteToStream(&stream, source, sizeof(source) - 1);
    fsCloseStream(&stream);
    ASSERT_EQ(written, sizeof(source) - 1);
    hz::GPURayTracingPipeline pipeline =
        context.createRayTracingPipeline({ .sourceDirectory = RD_OTHER_FILES,
                                           .pFileName = "dxr-regression.slang",
                                           .pRayGeneration = "RayGeneration",
                                           .missShaders = { "Miss" },
                                           .hitGroups = { { .pName = "TriangleHit", .pClosestHit = "ClosestHit" } },
                                           .payloadSize = 4,
                                           .maxRecursionDepth = 1,
                                           .rootConstantSize = 8 });
    ASSERT_TRUE(pipeline.isValid());
    const char      incrementSource[] = R"hlsl(
        cbuffer RootConstant0 { uint outputIndex; };
        [numthreads(2,1,1)] void CSMain(uint3 id : SV_DispatchThreadID) {
            RWStructuredBuffer<uint> output = ResourceDescriptorHeap[outputIndex];
            output[id.x] += 1000;
        }
    )hlsl";
    hz::GPUPipeline increment = context.createComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP,
                                                                                              .pSource = incrementSource,
                                                                                              .sourceSize = sizeof(incrementSource) - 1,
                                                                                              .pEntryPoint = "CSMain" } } } });
    ASSERT_TRUE(increment.isValid());
    const float                             positions[] = { -1, -1, 0, 1, -1, 0, 0, 1, 0 };
    const uint32_t                          indices[] = { 0, 1, 2 };
    hz::GPUBuffer                           vertices = context.createBuffer({ .size = sizeof(positions),
                                                                              .elementCount = 9,
                                                                              .pInitialData = positions,
                                                                              .initialDataSize = sizeof(positions),
                                                                              .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                                              .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                                                              .descriptors = DESCRIPTOR_TYPE_BUFFER_RAW });
    hz::GPUBuffer                           indexBuffer = context.createBuffer({ .size = sizeof(indices),
                                                                                 .elementCount = 3,
                                                                                 .pInitialData = indices,
                                                                                 .initialDataSize = sizeof(indices),
                                                                                 .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                                                 .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                                                                 .descriptors = DESCRIPTOR_TYPE_BUFFER_RAW });
    const hz::AccelerationStructureGeometry geometry = { .pVertexBuffer = &vertices,
                                                         .pIndexBuffer = &indexBuffer,
                                                         .vertexCount = 3,
                                                         .indexCount = 3 };
    hz::GPUAccelerationStructure            bottom = context.createAccelerationStructure({ .geometries = { &geometry, 1 } });
    ASSERT_TRUE(bottom.isValid());
    hz::AccelerationStructureInstance instance = { .pBottom = &bottom,
                                                   .instanceId = 23,
                                                   .flags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE };
    hz::GPUAccelerationStructure      top =
        context.createAccelerationStructure({ .type = ACCELERATION_STRUCTURE_TYPE_TOP,
                                              .flags = AccelerationStructureBuildFlags(ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE |
                                                                                       ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE),
                                              .instances = { &instance, 1 } });
    ASSERT_TRUE(top.isValid());
    hz::GPUBuffer output = context.createBuffer({ .size = 8,
                                                  .elementCount = 2,
                                                  .structStride = 4,
                                                  .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                  .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                  .descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer readback = context.createBuffer({ .size = 8,
                                                    .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                    .startState = RESOURCE_STATE_COPY_DEST,
                                                    .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    const auto    validateDevice = [&](const char* phase)
    {
        ID3D12Device* device = nullptr;
        output.get()->dx.pResource->GetDevice(IID_PPV_ARGS(&device));
        if (!device)
        {
            return E_FAIL;
        }
        const HRESULT    removed = device->GetDeviceRemovedReason();
        ID3D12InfoQueue* info = nullptr;
        device->QueryInterface(IID_PPV_ARGS(&info));
        if (info)
        {
            for (UINT64 i = 0; i < info->GetNumStoredMessagesAllowedByRetrievalFilter(); ++i)
            {
                SIZE_T size = 0;
                info->GetMessage(i, nullptr, &size);
                std::vector<uint8_t> bytes(size);
                D3D12_MESSAGE*       message = (D3D12_MESSAGE*)bytes.data();
                info->GetMessage(i, message, &size);
                if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR)
                {
                    ADD_FAILURE() << phase << ": " << message->pDescription;
                }
                else if (message->Severity == D3D12_MESSAGE_SEVERITY_WARNING)
                {
                    fprintf(stderr, "%s: %s\n", phase, message->pDescription);
                }
            }
            info->ClearStoredMessages();
            info->Release();
        }
        if (FAILED(removed))
        {
            ID3D12DeviceRemovedExtendedData* dred = nullptr;
            if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dred))))
            {
                D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs = {};
                if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput(&breadcrumbs)))
                {
                    for (const D3D12_AUTO_BREADCRUMB_NODE* node = breadcrumbs.pHeadAutoBreadcrumbNode; node; node = node->pNext)
                    {
                        const UINT completed = node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0;
                        ADD_FAILURE() << phase << " DRED completed " << completed << '/' << node->BreadcrumbCount;
                        const UINT first = completed > 4 ? completed - 4 : 0;
                        for (UINT i = first; node->pCommandHistory && i < node->BreadcrumbCount && i < completed + 4; ++i)
                        {
                            ADD_FAILURE() << "DRED operation[" << i << "] = " << unsigned(node->pCommandHistory[i]);
                        }
                    }
                }
                D3D12_DRED_PAGE_FAULT_OUTPUT pageFault = {};
                if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&pageFault)))
                {
                    ADD_FAILURE() << phase << " DRED page fault VA: " << std::hex << pageFault.PageFaultVA;
                }
                dred->Release();
            }
        }
        device->Release();
        return removed;
    };
    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        hz::CommandList& build = context.acquireCommandList();
        if (!pass)
        {
            build.buildAccelerationStructure(bottom);
        }
        instance.transform[3] = pass ? 4.0f : 0.0f;
        build.buildAccelerationStructure(top, { &instance, 1 }, pass != 0);
        context.wait(context.submit(build));
        ASSERT_EQ(validateDevice("AS build"), S_OK) << "pass " << pass;

        hz::CommandList& commands = context.acquireCommandList();
        commands.setPipeline(pipeline);
        commands.setRootConstant({ top.getSrvIndex(), output.getUavIndex() });
        commands.traceRays(pipeline, 2, 1, 1, { .storageBuffers = { &output } });
        commands.setPipeline(increment);
        commands.setRootConstant({ output.getUavIndex() });
        commands.dispatch(1, 1, 1, { .storageBuffers = { &output } });
        commands.copyBuffer(readback, 0, output, 0, 8);
        context.wait(context.submit(commands));
        const HRESULT removed = validateDevice("TraceRays -> Compute");
        ASSERT_EQ(removed, S_OK) << std::hex << (uint32_t)removed << "; pass " << pass;
        const uint32_t* values = (const uint32_t*)readback.get()->pCpuMappedAddress;
        ASSERT_NE(values, nullptr);
        EXPECT_EQ(values[0], pass ? 1007u : 1123u);
        EXPECT_EQ(values[1], pass ? 1123u : 1007u);
    }
}

static const char kCookbookTextureReadback[] = R"hlsl(
    cbuffer RootConstant0 { uint sourceIndex; uint outputIndex; uint offset; };
    [numthreads(8,8,1)] void CSMain(uint3 id : SV_DispatchThreadID) {
        Texture2D<float4> source = ResourceDescriptorHeap[sourceIndex];
        uint width,height; source.GetDimensions(width,height);
        if(id.x >= width || id.y >= height) return;
        RWStructuredBuffer<float4> output = ResourceDescriptorHeap[outputIndex];
        output[offset + id.y * width + id.x] = source.Load(int3(id.xy,0));
    }
)hlsl";

TEST_F(SceneAssetLoaderTest, CookbookRayShadowRejectsMaskedAlphaAndKeepsOpaqueOccluders)
{
    hz::RenderContext context({ .pAppName = "CookbookRayShadowTest", .enableGpuValidation = true });
    if (!context.supportsRayTracing())
    {
        GTEST_SKIP() << "Selected device does not support inline ray queries";
    }
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, shaders.string().c_str());
    hz::GPUPipeline shadow =
        context.createComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } },
                                                        .sourceDirectory = RD_OTHER_FILES,
                                                        .pFileName = "RTShadowAO.slang" } });
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, directory.string().c_str());
    hz::GPUPipeline reader =
        context.createComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP,
                                                                      .pSource = kCookbookTextureReadback,
                                                                      .sourceSize = sizeof(kCookbookTextureReadback) - 1,
                                                                      .pEntryPoint = "CSMain" } } } });
    ASSERT_TRUE(shadow.isValid() && reader.isValid());
    const auto input = [&](const void* data, uint32_t size, uint32_t stride = 0)
    {
        return context.createBuffer({ .size = size,
                                      .elementCount = stride ? size / stride : size / 4,
                                      .structStride = stride,
                                      .pInitialData = data,
                                      .initialDataSize = size,
                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                      .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                      .descriptors = stride ? DESCRIPTOR_TYPE_BUFFER : DESCRIPTOR_TYPE_BUFFER_RAW });
    };
    const auto image = [&](hz::Format format, bool target)
    {
        return context.createTexture({ .width = 1,
                                       .height = 1,
                                       .format = format,
                                       .startState = target ? RESOURCE_STATE_RENDER_TARGET : RESOURCE_STATE_UNORDERED_ACCESS,
                                       .descriptors = DescriptorType(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE),
                                       .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                       .renderTarget = target });
    };
    hz::GPUTexture normal = image(hz::Format::R32G32B32A32_SFLOAT, true), depth = image(hz::Format::R32_SFLOAT, true);
    hz::GPUTexture alpha = image(hz::Format::R32G32B32A32_SFLOAT, true), noise = image(hz::Format::R32G32B32A32_SFLOAT, true);
    hz::GPUTexture result = image(hz::Format::R8G8_UNORM, false);
    hz::GPUSampler sampler = context.createSampler();
    struct FrameData
    {
        Matrix4 current, previous, inverse;
        Vector4 eye;
    };
    const FrameData         camera = { Matrix4::identity(), Matrix4::identity(), Matrix4::identity(), Vector4(0, 0, -1, 1) };
    const LightingFrameData lighting = { .lightDirection = Vector4(0, 0, -1, 1) };
    hz::GPUBuffer  frame = input(&camera, sizeof(camera), sizeof(camera)), light = input(&lighting, sizeof(lighting), sizeof(lighting));
    const float    positions[] = { -1, -1, 0.5f, 3, -1, 0.5f, -1, 3, 0.5f };
    // One uint16 triangle, padded so the production RAW decoder can read its final word.
    const uint16_t indices[] = { 0, 1, 2, 0 };
    const uint32_t texcoords[] = { 0, 0, 0 };
    hz::GPUBuffer  vertices = input(positions, sizeof(positions)), indexBuffer = input(indices, sizeof(indices));
    hz::GPUBuffer  uv = input(texcoords, sizeof(texcoords));
    hz::GpuMaterialRecord materials[2];
    for (uint32_t i = 0; i < 2; ++i)
    {
        materials[i].id = { i, 1 };
        materials[i].baseColorTexture = { 3, 9 };
        materials[i].alphaMode = i;
        materials[i].alphaCutoff = 0.5f;
    }
    hz::GpuTextureRecord textures[4];
    textures[3] = { .id = { 3, 9 }, .srv = alpha.getSrvIndex() };
    GpuDrawRecord records[7] = {};
    records[5] = { .material = 0,
                   .alphaMode = 0,
                   .positions = vertices.getSrvIndex(),
                   .normals = uv.getSrvIndex(),
                   .texcoords = uv.getSrvIndex(),
                   .indices = indexBuffer.getSrvIndex(),
                   .indexStride = 2,
                   .indexCount = 3 };
    records[6] = records[5];
    records[6].material = records[6].alphaMode = 1;
    hz::GPUBuffer materialBuffer = input(materials, sizeof(materials)), textureBuffer = input(textures, sizeof(textures));
    hz::GPUBuffer drawBuffer = input(records, sizeof(records));
    hz::AccelerationStructureGeometry geometry = { .pVertexBuffer = &vertices,
                                                   .pIndexBuffer = &indexBuffer,
                                                   .vertexCount = 3,
                                                   .indexCount = 3,
                                                   .indexType = INDEX_TYPE_UINT16 };
    hz::GPUAccelerationStructure      opaque = context.createAccelerationStructure({ .geometries = { &geometry, 1 } });
    geometry.opaque = false;
    hz::GPUAccelerationStructure            masked = context.createAccelerationStructure({ .geometries = { &geometry, 1 } });
    const hz::AccelerationStructureInstance instances[] = {
        { .pBottom = &opaque, .instanceId = 5, .flags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE },
        { .pBottom = &masked, .instanceId = 6, .flags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE },
    };
    hz::GPUAccelerationStructure opaqueTop =
        context.createAccelerationStructure({ .type = ACCELERATION_STRUCTURE_TYPE_TOP, .instances = { instances, 1 } });
    hz::GPUAccelerationStructure maskedTop =
        context.createAccelerationStructure({ .type = ACCELERATION_STRUCTURE_TYPE_TOP, .instances = { instances + 1, 1 } });
    ASSERT_TRUE(opaque.isValid() && masked.isValid() && opaqueTop.isValid() && maskedTop.isValid());
    hz::GPUBuffer output = context.createBuffer({ .size = 16,
                                                  .elementCount = 1,
                                                  .structStride = 16,
                                                  .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                  .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                  .descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer readback = context.createBuffer({ .size = 16,
                                                    .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                    .startState = RESOURCE_STATE_COPY_DEST,
                                                    .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    struct ShadowConstants
    {
        uint32_t frame, lighting, draws, materials, textures, sampler, top, normal, depth, output, noise;
        uint32_t environment = UINT32_MAX, irradiance = UINT32_MAX, brdf = UINT32_MAX, environmentSampler = UINT32_MAX;
        uint32_t shadows = 1, ao = 0, samples = 1, phase = 0, alphaMethod = 1, retraceDepth = 3;
        float    aoRadius = 2, aoPower = 1, shadowRadius = 0;
    } constants = { frame.getSrvIndex(),
                    light.getSrvIndex(),
                    drawBuffer.getSrvIndex(),
                    materialBuffer.getSrvIndex(),
                    textureBuffer.getSrvIndex(),
                    sampler.getIndex(),
                    0,
                    normal.getSrvIndex(),
                    depth.getSrvIndex(),
                    result.getUavIndex(),
                    noise.getSrvIndex() };
    // Three alpha-test modes, then the reference AO path which deliberately forces opaque hits.
    for (uint32_t caseIndex = 0; caseIndex < 12; ++caseIndex)
    {
        SCOPED_TRACE(caseIndex);
        const uint32_t scenario = caseIndex % 3;
        constants.alphaMethod = (caseIndex / 3) % 3;
        constants.shadows = caseIndex < 9;
        constants.ao = caseIndex >= 9;
        hz::CommandList& commands = context.acquireCommandList();
        if (!caseIndex)
        {
            commands.buildAccelerationStructure(opaque);
            commands.buildAccelerationStructure(masked);
            commands.buildAccelerationStructure(opaqueTop);
            commands.buildAccelerationStructure(maskedTop);
        }
        const auto clear = [&](const hz::GPUTexture& texture, ClearValue value)
        {
            commands.beginRendering({ .colorAttachments = { { .pTexture = &texture,
                                                              .loadAction = LOAD_ACTION_CLEAR,
                                                              .storeAction = STORE_ACTION_STORE,
                                                              .clearValue = value } } });
            commands.endRendering();
        };
        clear(normal, { .r = 0.5f, .g = 0.5f, .a = 1 }); // Octahedral +Z.
        clear(depth, { .r = 0.2f });
        clear(noise, { .r = 0.5f, .g = 0.5f });
        clear(alpha, { .a = scenario == 2 ? 0.75f : 0.25f });
        constants.top = scenario ? maskedTop.getSrvIndex() : opaqueTop.getSrvIndex();
        commands.setPipeline(shadow);
        commands.setRootConstant({ reinterpret_cast<const uint32_t*>(&constants), sizeof(constants) / 4 });
        commands.dispatch(1, 1, 1,
                          { .sampledTextures = { &normal, &depth, &noise, &alpha },
                            .storageTextures = { &result },
                            .buffers = { &frame, &light, &drawBuffer, &materialBuffer, &textureBuffer, &vertices, &indexBuffer, &uv } });
        commands.setPipeline(reader);
        commands.setRootConstant({ result.getSrvIndex(), output.getUavIndex(), 0 });
        commands.dispatch(1, 1, 1, { .sampledTextures = { &result }, .storageBuffers = { &output } });
        commands.copyBuffer(readback, 0, output, 0, 16);
        context.wait(context.submit(commands));
        const float* values = (const float*)readback.get()->pCpuMappedAddress;
        ASSERT_NE(values, nullptr);
        EXPECT_FLOAT_EQ(values[0], constants.ao || (constants.alphaMethod != 0 && scenario == 1) ? 1.0f : 0.0f);
        EXPECT_FLOAT_EQ(values[1], constants.ao ? 0.0f : 1.0f);
    }

    // Reuse the same scene to exercise the production raster shadow alpha test as well.
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, shaders.string().c_str());
    hz::GPUPipeline raster =
        context.createGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSIndirect" },
                                                                     { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSShadow" } },
                                                         .sourceDirectory = RD_OTHER_FILES,
                                                         .pFileName = "ShadowMap.slang" },
                                         .depth = { .depthTest = true, .depthWrite = true, .depthFunc = CMP_LEQUAL },
                                         .depthStencilFormat = hz::Format::D16_UNORM,
                                         .indirectDrawId = true });
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, directory.string().c_str());
    ASSERT_TRUE(raster.isValid());
    const hz::RenderInstance identity;
    hz::GPUBuffer            instanceBuffer = input(&identity, sizeof(identity));
    const uint32_t           arguments[2][5] = { { 5, 3, 1, 0, 0 }, { 6, 3, 1, 0, 0 } };
    hz::GPUBuffer            indirect = context.createBuffer({ .size = sizeof(arguments),
                                                               .pInitialData = arguments,
                                                               .initialDataSize = sizeof(arguments),
                                                               .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                               .startState = RESOURCE_STATE_INDIRECT_ARGUMENT,
                                                               .descriptors = DESCRIPTOR_TYPE_INDIRECT_BUFFER });
    hz::GPUTexture           shadowDepth = context.createTexture({ .width = 1,
                                                                   .height = 1,
                                                                   .format = hz::Format::D16_UNORM,
                                                                   .startState = RESOURCE_STATE_DEPTH_WRITE,
                                                                   .descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                                                   .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                                                   .renderTarget = true });
    ASSERT_TRUE(instanceBuffer.isValid() && indirect.isValid() && shadowDepth.isValid());
    for (uint32_t scenario = 0; scenario < 3; ++scenario)
    {
        SCOPED_TRACE(scenario);
        hz::CommandList& commands = context.acquireCommandList();
        commands.beginRendering({ .colorAttachments = { { .pTexture = &alpha,
                                                          .loadAction = LOAD_ACTION_CLEAR,
                                                          .storeAction = STORE_ACTION_STORE,
                                                          .clearValue = { .a = scenario == 2 ? 0.75f : 0.25f } } } });
        commands.endRendering();
        commands.setPipeline(raster);
        commands.beginRendering(
            { .depthAttachment = { .pTexture = &shadowDepth,
                                   .loadAction = LOAD_ACTION_CLEAR,
                                   .storeAction = STORE_ACTION_STORE,
                                   .clearValue = { .depth = 1 } } },
            { .sampledTextures = { &alpha },
              .buffers = { &frame, &instanceBuffer, &materialBuffer, &textureBuffer, &drawBuffer, &vertices, &indexBuffer, &uv },
              .indirectBuffers = { &indirect } });
        commands.setViewport(0, 0, 1, 1);
        commands.setScissor(0, 0, 1, 1);
        commands.setRootConstant({ 0, 0, 0, frame.getSrvIndex(), instanceBuffer.getSrvIndex(), materialBuffer.getSrvIndex(),
                                   textureBuffer.getSrvIndex(), sampler.getIndex() });
        commands.setRootConstant({ drawBuffer.getSrvIndex() }, 11);
        commands.drawIndirect(indirect, scenario ? 20 : 0, 1);
        commands.endRendering();
        commands.setPipeline(reader);
        commands.setRootConstant({ shadowDepth.getSrvIndex(), output.getUavIndex(), 0 });
        commands.dispatch(1, 1, 1, { .sampledTextures = { &shadowDepth }, .storageBuffers = { &output } });
        commands.copyBuffer(readback, 0, output, 0, 16);
        context.wait(context.submit(commands));
        const float* values = (const float*)readback.get()->pCpuMappedAddress;
        ASSERT_NE(values, nullptr);
        EXPECT_NEAR(values[0], scenario == 1 ? 1.0f : 0.5f, 2e-5f);
    }
}

TEST_F(SceneAssetLoaderTest, CookbookPpllSortsDepthRejectsOccludedFragmentsAndBoundsOverflow)
{
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, shaders.string().c_str());
    hz::RenderContext context({ .pAppName = "CookbookPpllTest", .enableGpuValidation = true });
    hz::GPUPipeline   gather =
        context.createGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                                                     { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSTransparent" } },
                                                         .sourceDirectory = RD_OTHER_FILES,
                                                         .pFileName = "Transparency.slang" },
                                         .depth = { .depthTest = true, .depthWrite = false, .depthFunc = CMP_LEQUAL },
                                         .depthStencilFormat = hz::Format::D32_SFLOAT });
    const auto compute = [&](const char* entry)
    {
        return context.createComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = entry } },
                                                               .sourceDirectory = RD_OTHER_FILES,
                                                               .pFileName = "Transparency.slang" } });
    };
    hz::GPUPipeline clear = compute("ClearMain"), resolve = compute("ResolveMain");
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, directory.string().c_str());
    hz::GPUPipeline reader =
        context.createComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP,
                                                                      .pSource = kCookbookTextureReadback,
                                                                      .sourceSize = sizeof(kCookbookTextureReadback) - 1,
                                                                      .pEntryPoint = "CSMain" } } } });
    ASSERT_TRUE(gather.isValid() && clear.isValid() && resolve.isValid() && reader.isValid());
    const auto input = [&](const void* data, uint32_t size, uint32_t stride = 0)
    {
        return context.createBuffer({ .size = size,
                                      .elementCount = stride ? size / stride : size / 4,
                                      .structStride = stride,
                                      .pInitialData = data,
                                      .initialDataSize = size,
                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                      .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                      .descriptors = stride ? DESCRIPTOR_TYPE_BUFFER : DESCRIPTOR_TYPE_BUFFER_RAW });
    };
    struct FrameData
    {
        Matrix4 vp, previousVP, inverseVP;
        Vector4 eye;
    };
    const FrameData         camera = { Matrix4::identity(), Matrix4::identity(), Matrix4::identity(), Vector4(0, 0, -1, 1) };
    const LightingFrameData lighting;
    hz::RenderInstance      instances[2];
    instances[0].current = Matrix4::translation(Vector3(0, 0, 0.2f));
    instances[1].current = Matrix4::translation(Vector3(0, 0, 0.7f));
    hz::GpuMaterialRecord materials[2];
    for (hz::GpuMaterialRecord& material : materials)
    {
        material.alphaMode = 2;
        material.baseColorFactor[3] = 0.5f;
    }
    materials[0].emissiveFactor[0] = materials[1].emissiveFactor[1] = 1;
    const float    positions[] = { -1, -1, 0, 3, -1, 0, -1, 3, 0 };
    const uint32_t normals[] = { 0x80008000, 0x80008000, 0x80008000 }, texcoords[] = { 0, 0, 0 };
    hz::GPUBuffer  frame = input(&camera, sizeof(camera), sizeof(camera)), light = input(&lighting, sizeof(lighting), sizeof(lighting));
    hz::GPUBuffer  instanceBuffer = input(instances, sizeof(instances)), materialBuffer = input(materials, sizeof(materials));
    hz::GPUBuffer  vertices = input(positions, sizeof(positions)), normal = input(normals, sizeof(normals)),
                  uv = input(texcoords, sizeof(texcoords));
    hz::GPUSampler sampler = context.createSampler();
    const auto     image = [&](hz::Format format, bool target)
    {
        return context.createTexture({ .width = 8,
                                       .height = 8,
                                       .format = format,
                                       .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                       .descriptors = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE),
                                       .renderTarget = target });
    };
    hz::GPUTexture heads = image(hz::Format::R32_UINT, false), result = image(hz::Format::R32G32B32A32_SFLOAT, false);
    hz::GPUTexture opaque = image(hz::Format::R32G32B32A32_SFLOAT, true);
    hz::GPUTexture depth = context.createTexture(
        { .width = 8, .height = 8, .format = hz::Format::D32_SFLOAT, .startState = RESOURCE_STATE_DEPTH_WRITE, .renderTarget = true });
    hz::GPUBuffer counters =
        context.createBuffer({ .size = 16,
                               .elementCount = 4,
                               .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                               .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                               .descriptors = (DescriptorType)(DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW) });
    hz::GPUBuffer pixels = context.createBuffer({ .size = 64 * 16,
                                                  .elementCount = 64,
                                                  .structStride = 16,
                                                  .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                  .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                  .descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer readback = context.createBuffer({ .size = 64 * 16 + 16,
                                                    .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                    .startState = RESOURCE_STATE_COPY_DEST,
                                                    .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    for (uint32_t scenario = 0; scenario < 4; ++scenario)
    {
        SCOPED_TRACE(scenario);
        const uint32_t capacity = scenario == 3 ? 1 : 128;
        hz::GPUBuffer  nodes =
            context.createBuffer({ .size = capacity * 16,
                                   .elementCount = capacity * 4,
                                   .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                   .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                   .descriptors = (DescriptorType)(DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW) });
        hz::CommandList& commands = context.acquireCommandList();
        commands.beginRendering({ .colorAttachments = { { .pTexture = &opaque,
                                                          .loadAction = LOAD_ACTION_CLEAR,
                                                          .storeAction = STORE_ACTION_STORE,
                                                          .clearValue = { .b = 1, .a = 1 } } } });
        commands.endRendering();
        uint32_t constants[28];
        for (uint32_t& value : constants)
        {
            value = UINT32_MAX;
        }
        constants[3] = frame.getSrvIndex();
        constants[4] = instanceBuffer.getSrvIndex();
        constants[5] = materialBuffer.getSrvIndex();
        constants[7] = sampler.getIndex();
        constants[8] = vertices.getSrvIndex();
        constants[9] = normal.getSrvIndex();
        constants[10] = uv.getSrvIndex();
        constants[12] = heads.getUavIndex();
        constants[13] = counters.getUavIndex();
        constants[14] = nodes.getUavIndex();
        constants[15] = capacity;
        constants[16] = light.getSrvIndex();
        commands.setPipeline(clear);
        commands.setRootConstant(constants);
        commands.dispatch(1, 1, 1, { .storageTextures = { &heads }, .storageBuffers = { &counters } });
        commands.beginRendering({ .depthAttachment = { .pTexture = &depth,
                                                       .loadAction = LOAD_ACTION_CLEAR,
                                                       .storeAction = STORE_ACTION_STORE,
                                                       .clearValue = { .depth = scenario == 2 ? 0.5f : 1.0f } } },
                                { .storageTextures = { &heads },
                                  .buffers = { &frame, &light, &instanceBuffer, &materialBuffer, &vertices, &normal, &uv },
                                  .storageBuffers = { &counters, &nodes } });
        commands.setViewport(0, 0, 8, 8);
        commands.setScissor(0, 0, 8, 8);
        commands.setPipeline(gather);
        for (uint32_t draw = 0; draw < 2; ++draw)
        {
            constants[0] = constants[1] = scenario == 1 ? 1 - draw : draw;
            commands.setRootConstant(constants);
            commands.draw(3);
        }
        commands.endRendering();
        constants[12] = heads.getSrvIndex();
        constants[14] = nodes.getSrvIndex();
        constants[23] = opaque.getSrvIndex();
        constants[24] = constants[25] = constants[27] = 0;
        constants[26] = result.getUavIndex();
        commands.setPipeline(resolve);
        commands.setRootConstant(constants);
        commands.dispatch(1, 1, 1,
                          { .sampledTextures = { &heads, &opaque },
                            .storageTextures = { &result },
                            .buffers = { &nodes },
                            .storageBuffers = { &counters } });
        commands.setPipeline(reader);
        commands.setRootConstant({ result.getSrvIndex(), pixels.getUavIndex(), 0 });
        commands.dispatch(1, 1, 1, { .sampledTextures = { &result }, .storageBuffers = { &pixels } });
        commands.copyBuffer(readback, 0, pixels, 0, 64 * 16);
        commands.copyBuffer(readback, 64 * 16, counters, 0, 16);
        context.wait(context.submit(commands));
        const float*    color = (const float*)readback.get()->pCpuMappedAddress;
        const uint32_t* stats = (const uint32_t*)((const uint8_t*)color + 64 * 16);
        EXPECT_EQ(stats[0], scenario == 2 ? 64u : 128u);
        EXPECT_EQ(stats[1], scenario == 3 ? 127u : 0u);
        EXPECT_EQ(stats[2], 0u);
        double sum[3] = {};
        for (uint32_t pixel = 0; pixel < 64; ++pixel)
        {
            for (uint32_t channel = 0; channel < 3; ++channel)
            {
                sum[channel] += color[pixel * 4 + channel];
            }
            if (scenario != 3)
            {
                EXPECT_NEAR(color[pixel * 4], 0.5, 1e-5);
                EXPECT_NEAR(color[pixel * 4 + 1], scenario == 2 ? 0.0 : 0.25, 1e-5);
                EXPECT_NEAR(color[pixel * 4 + 2], scenario == 2 ? 0.5 : 0.25, 1e-5);
            }
        }
        if (scenario == 3)
        {
            EXPECT_NEAR(sum[0] + sum[1], 0.5, 1e-5);
            EXPECT_NEAR(sum[2], 63.5, 1e-5);
        }
    }
}

TEST_F(SceneAssetLoaderTest, CookbookPostExposureToneMapsAndBloomMatchReferenceValues)
{
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, shaders.string().c_str());
    hz::RenderContext context({ .pAppName = "CookbookPostTest", .enableGpuValidation = true });
    const auto        compute = [&](const char* file, const char* entry)
    {
        return context.createComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = entry } },
                                                               .sourceDirectory = RD_OTHER_FILES,
                                                               .pFileName = file } });
    };
    hz::GPUPipeline initialize = compute("PostLuminance.slang", "CSInitialize");
    hz::GPUPipeline reduce = compute("PostLuminance.slang", "CSReduce"), adapt = compute("PostLuminance.slang", "CSAdapt");
    hz::GPUPipeline bright = compute("PostBrightPass.slang", "CSMain"), bloom = compute("PostBloom.slang", "CSMain");
    hz::GPUPipeline tone =
        context.createGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                                                     { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                                                         .sourceDirectory = RD_OTHER_FILES,
                                                         .pFileName = "PostToneMap.slang" },
                                         .colorTargets = { { .format = hz::Format::R32G32B32A32_SFLOAT } } });
    hz::GPUPipeline deferred =
        context.createGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                                                     { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                                                         .sourceDirectory = RD_OTHER_FILES,
                                                         .pFileName = "DeferredLighting.slang" },
                                         .colorTargets = { { .format = hz::Format::R32G32B32A32_SFLOAT } } });
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, directory.string().c_str());
    const char seedSource[] = R"hlsl(
        cbuffer RootConstant0 { uint outputIndex; };
        [numthreads(8,8,1)] void CSLogGrid(uint3 id : SV_DispatchThreadID) {
            RWTexture2D<float> output = ResourceDescriptorHeap[outputIndex];
            uint w,h; output.GetDimensions(w,h);
            if(id.x < w && id.y < h) output[id.xy] = 2.0 * (id.x + 2 * id.y);
        }
        [numthreads(8,8,1)] void CSImpulse(uint3 id : SV_DispatchThreadID) {
            RWTexture2D<float4> output = ResourceDescriptorHeap[outputIndex];
            uint w,h; output.GetDimensions(w,h);
            if(id.x < w && id.y < h) output[id.xy] = float4(all(id.xy == uint2(4,4)) ? 12.0 : 0.0,0,0,1);
        }
    )hlsl";
    const auto helper = [&](const char* source, uint32_t size, const char* entry)
    {
        return context.createComputePipeline(
            { .shaderDesc = {
                  .stages = { { .stage = SHADER_STAGE_COMP, .pSource = source, .sourceSize = size, .pEntryPoint = entry } } } });
    };
    hz::GPUPipeline seedLogs = helper(seedSource, sizeof(seedSource) - 1, "CSLogGrid");
    hz::GPUPipeline seedImpulse = helper(seedSource, sizeof(seedSource) - 1, "CSImpulse");
    hz::GPUPipeline reader = helper(kCookbookTextureReadback, sizeof(kCookbookTextureReadback) - 1, "CSMain");
    ASSERT_TRUE(initialize.isValid() && reduce.isValid() && adapt.isValid() && bright.isValid() && bloom.isValid() && tone.isValid() &&
                seedLogs.isValid() && seedImpulse.isValid() && reader.isValid() && deferred.isValid());
    const auto image = [&](uint32_t width, uint32_t height, hz::Format format)
    {
        return context.createTexture({ .width = width,
                                       .height = height,
                                       .format = format,
                                       .startState = RESOURCE_STATE_RENDER_TARGET,
                                       .descriptors = (DescriptorType)(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE),
                                       .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                       .renderTarget = true });
    };
    constexpr hz::Format rgba = hz::Format::R32G32B32A32_SFLOAT, scalar = hz::Format::R32_SFLOAT;
    hz::GPUTexture       source = image(1, 1, rgba), black = image(1, 1, rgba), average = image(1, 1, scalar), mapped = image(1, 1, rgba);
    hz::GPUTexture       brightOutput = image(2, 2, rgba), logOutput = image(2, 2, scalar), logGrid = image(2, 2, scalar);
    hz::GPUTexture       reduced = image(1, 1, scalar), previous = image(1, 1, scalar), adapted = image(1, 1, scalar);
    hz::GPUTexture       impulse = image(9, 9, rgba), blurred = image(9, 9, rgba);
    hz::GPUSampler       sampler = context.createSampler(
        { .addressU = ADDRESS_MODE_CLAMP_TO_EDGE, .addressV = ADDRESS_MODE_CLAMP_TO_EDGE, .addressW = ADDRESS_MODE_CLAMP_TO_EDGE });
    hz::GPUBuffer  output = context.createBuffer({ .size = 256 * 16,
                                                   .elementCount = 256,
                                                   .structStride = 16,
                                                   .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                   .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                   .descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer  readback = context.createBuffer({ .size = 256 * 16,
                                                     .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                     .startState = RESOURCE_STATE_COPY_DEST,
                                                     .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::GPUTexture peakNormal = image(1, 1, rgba), peakBase = image(1, 1, rgba);
    struct PeakFrame
    {
        Matrix4 vp, previous, inverse;
        Vector4 eye;
    };
    const PeakFrame   peakCamera = { Matrix4::identity(), Matrix4::identity(), Matrix4::identity(), Vector4(0, 0, 2, 1) };
    LightingFrameData peakLight;
    peakLight.lightDirection = Vector4(0, 0, -1, 100000);
    const auto upload = [&](const void* data, uint32_t size)
    {
        return context.createBuffer({ .size = size,
                                      .elementCount = 1,
                                      .structStride = size,
                                      .pInitialData = data,
                                      .initialDataSize = size,
                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                      .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                      .descriptors = DESCRIPTOR_TYPE_BUFFER });
    };
    hz::GPUBuffer    peakFrame = upload(&peakCamera, sizeof(peakCamera)), peakLighting = upload(&peakLight, sizeof(peakLight));
    hz::CommandList& commands = context.acquireCommandList();
    const auto       clear = [&](const hz::GPUTexture& texture, float value)
    {
        commands.beginRendering({ .colorAttachments = { { .pTexture = &texture,
                                                          .loadAction = LOAD_ACTION_CLEAR,
                                                          .storeAction = STORE_ACTION_STORE,
                                                          .clearValue = { .r = value, .g = value, .b = value, .a = 1 } } } });
        commands.endRendering();
    };
    clear(source, 1.2f);
    clear(black, 0);
    clear(average, 0.18f);
    const auto read = [&](const hz::GPUTexture& texture, uint32_t offset)
    {
        commands.setPipeline(reader);
        commands.setRootConstant({ texture.getSrvIndex(), output.getUavIndex(), offset });
        commands.dispatch((texture.getWidth() + 7) / 8, (texture.getHeight() + 7) / 8, 1,
                          { .sampledTextures = { &texture }, .storageBuffers = { &output } });
    };
    struct ToneConstants
    {
        uint32_t source, average, bloom, sampler, mode;
        float    exposure = 1, bloomStrength = 0, maxWhite = 2, P = 1, a = 1.05f, m = 0.1f, l = 0.8f, c = 3, b = 0, compression = 0.8f,
              desaturation = 0.15f;
        uint32_t outputMode = 0, encodeSRGB = 0;
        float    paperWhite = 200, peak = 1000;
    } toneConstants = { source.getSrvIndex(), average.getSrvIndex(), black.getSrvIndex(), sampler.getIndex(), 0 };
    for (uint32_t mode = 0; mode < 4; ++mode)
    {
        commands.beginRendering(
            { .colorAttachments = { { .pTexture = &mapped, .loadAction = LOAD_ACTION_DONTCARE, .storeAction = STORE_ACTION_STORE } } },
            { .sampledTextures = { &source, &average, &black } });
        commands.setViewport(0, 0, 1, 1);
        commands.setScissor(0, 0, 1, 1);
        commands.setPipeline(tone);
        toneConstants.mode = mode;
        commands.setRootConstant({ (const uint32_t*)&toneConstants, sizeof(toneConstants) / 4 });
        commands.draw(3);
        commands.endRendering();
        read(mapped, mode);
    }
    struct BrightConstants
    {
        uint32_t source, bright, log, sampler;
        float    exposure;
        uint32_t enabled, average, autoExposure;
    };
    const BrightConstants brightConstants = {
        source.getSrvIndex(), brightOutput.getUavIndex(), logOutput.getUavIndex(), sampler.getIndex(), 1, 1, average.getSrvIndex(), 0
    };
    commands.setPipeline(bright);
    commands.setRootConstant({ (const uint32_t*)&brightConstants, sizeof(brightConstants) / 4 });
    commands.dispatch(1, 1, 1, { .sampledTextures = { &source, &average }, .storageTextures = { &brightOutput, &logOutput } });
    read(brightOutput, 4);
    read(logOutput, 8);
    // Real deferred shader: N=V=L, white dielectric, roughness=.2, quantized Specular=8/15. No environment or shadow.
    commands.beginRendering({ .colorAttachments = { { .pTexture = &peakNormal,
                                                      .loadAction = LOAD_ACTION_CLEAR,
                                                      .storeAction = STORE_ACTION_STORE,
                                                      .clearValue = { .r = .5f, .g = .5f, .b = 51.0f / 1023.0f, .a = 2.0f / 3.0f } },
                                                    { .pTexture = &peakBase,
                                                      .loadAction = LOAD_ACTION_CLEAR,
                                                      .storeAction = STORE_ACTION_STORE,
                                                      .clearValue = { .r = 1, .g = 1, .b = 1, .a = 0 } } } });
    commands.endRendering();
    commands.setPipeline(deferred);
    commands.beginRendering(
        { .colorAttachments = { { .pTexture = &mapped, .loadAction = LOAD_ACTION_DONTCARE, .storeAction = STORE_ACTION_STORE } } },
        { .sampledTextures = { &black, &average, &peakNormal, &peakBase }, .buffers = { &peakFrame, &peakLighting } });
    commands.setViewport(0, 0, 1, 1);
    commands.setScissor(0, 0, 1, 1);
    uint32_t peakConstants[14];
    for (uint32_t& value : peakConstants)
    {
        value = UINT32_MAX;
    }
    peakConstants[0] = peakFrame.getSrvIndex();
    peakConstants[1] = peakLighting.getSrvIndex();
    peakConstants[2] = black.getSrvIndex();
    peakConstants[3] = peakNormal.getSrvIndex();
    peakConstants[4] = peakBase.getSrvIndex();
    peakConstants[5] = average.getSrvIndex();
    commands.setRootConstant(peakConstants);
    commands.draw(3);
    commands.endRendering();
    read(mapped, 15);
    struct LuminanceConstants
    {
        uint32_t input, output, previous;
        float    time;
    };
    LuminanceConstants luminanceConstants = { previous.getUavIndex(), adapted.getUavIndex(), 0, 0 };
    commands.setPipeline(initialize);
    commands.setRootConstant({ (const uint32_t*)&luminanceConstants, sizeof(luminanceConstants) / 4 });
    commands.dispatch(1, 1, 1, { .storageTextures = { &previous, &adapted } });
    read(previous, 12);
    commands.setPipeline(seedLogs);
    commands.setRootConstant({ logGrid.getUavIndex() });
    commands.dispatch(1, 1, 1, { .storageTextures = { &logGrid } });
    commands.setPipeline(reduce);
    luminanceConstants = { logGrid.getSrvIndex(), reduced.getUavIndex(), 0, 0 };
    commands.setRootConstant({ (const uint32_t*)&luminanceConstants, sizeof(luminanceConstants) / 4 });
    commands.dispatch(1, 1, 1, { .sampledTextures = { &logGrid }, .storageTextures = { &reduced } });
    read(reduced, 13);
    commands.setPipeline(adapt);
    luminanceConstants = { reduced.getSrvIndex(), adapted.getUavIndex(), previous.getSrvIndex(), 0.3f };
    commands.setRootConstant({ (const uint32_t*)&luminanceConstants, sizeof(luminanceConstants) / 4 });
    commands.dispatch(1, 1, 1, { .sampledTextures = { &reduced, &previous }, .storageTextures = { &adapted } });
    read(adapted, 14);
    commands.setPipeline(seedImpulse);
    commands.setRootConstant({ impulse.getUavIndex() });
    commands.dispatch(2, 2, 1, { .storageTextures = { &impulse } });
    for (uint32_t horizontal = 0; horizontal < 2; ++horizontal)
    {
        commands.setPipeline(bloom);
        commands.setRootConstant({ impulse.getSrvIndex(), blurred.getUavIndex(), sampler.getIndex(), horizontal });
        commands.dispatch(1, 1, 1, { .sampledTextures = { &impulse }, .storageTextures = { &blurred } });
        read(blurred, 16 + horizontal * 81);
    }
    // Real output shader: retain the HDR shoulder for gray 1.2 above paper white.
    toneConstants.mode = 0;
    toneConstants.paperWhite = 203;
    toneConstants.peak = 1000;
    for (uint32_t mode = 1; mode <= 2; ++mode)
    {
        toneConstants.outputMode = mode;
        commands.setPipeline(tone);
        commands.beginRendering(
            { .colorAttachments = { { .pTexture = &mapped, .loadAction = LOAD_ACTION_DONTCARE, .storeAction = STORE_ACTION_STORE } } },
            { .sampledTextures = { &source, &average, &black } });
        commands.setViewport(0, 0, 1, 1);
        commands.setScissor(0, 0, 1, 1);
        commands.setRootConstant({ reinterpret_cast<const uint32_t*>(&toneConstants), sizeof(toneConstants) / 4 });
        commands.draw(3);
        commands.endRendering();
        read(mapped, 177 + mode);
    }
    commands.copyBuffer(readback, 0, output, 0, 256 * 16);
    context.wait(context.submit(commands));
    const float* values = (const float*)readback.get()->pCpuMappedAddress;
    ASSERT_NE(values, nullptr);
    // Cookbook shared/Tonemap.h reference goldens: gray 1.2, white 2 and default Uchimura/PBR-neutral controls, linear SDR.
    const double golden[] = { 1.2, 0.7090909090909091, 0.9839406668663332, 0.91 };
    for (uint32_t mode = 0; mode < 4; ++mode)
    {
        for (uint32_t channel = 0; channel < 3; ++channel)
        {
            EXPECT_NEAR(values[mode * 4 + channel], golden[mode], 2e-5) << mode;
        }
    }
    for (uint32_t pixel = 0; pixel < 4; ++pixel)
    {
        for (uint32_t channel = 0; channel < 3; ++channel)
        {
            EXPECT_NEAR(values[(4 + pixel) * 4 + channel], 0.245, 2e-5);
        }
        EXPECT_NEAR(values[(8 + pixel) * 4], std::log2(1.2), 2e-5);
    }
    // Offline double goldens: shoulder -> 242.58323517119788 nits; ST2084 PQ and nits/80 respectively.
    const double hdrGolden[] = { 0.5993841391362755, 3.0322904396399735 };
    for (uint32_t mode = 0; mode < 2; ++mode)
    {
        for (uint32_t channel = 0; channel < 3; ++channel)
        {
            EXPECT_NEAR(values[(178 + mode) * 4 + channel], hdrGolden[mode], 2e-5) << mode << ":" << channel;
        }
    }
    // At normal incidence the reference GGX reduces analytically to (1-F0)/pi + F0/(4*pi*roughness^4).
    const double f0 = 0.08 * 8.0 / 15.0, pi = 3.141592653589793;
    const double peakExpected = 100000.0 * ((1.0 - f0) / pi + f0 / (4.0 * pi * std::pow(0.2, 4)));
    for (uint32_t channel = 0; channel < 3; ++channel)
    {
        EXPECT_TRUE(std::isfinite(values[15 * 4 + channel]));
        EXPECT_GT(values[15 * 4 + channel], 65504.0f);
        EXPECT_NEAR(values[15 * 4 + channel], peakExpected, peakExpected * 2e-5);
    }
    EXPECT_FLOAT_EQ(values[12 * 4], 50.0f);
    EXPECT_FLOAT_EQ(values[13 * 4], 3.0f); // log2(1),log2(4),log2(16),log2(64): geometric mean is 8, not the arithmetic mean.
    EXPECT_NEAR(values[14 * 4], 50.0 + (8.0 - 50.0) * (1.0 - std::exp(-0.3)), 1e-4);
    for (uint32_t horizontal = 0; horizontal < 2; ++horizontal)
    {
        double energy = 0;
        for (int y = 0; y < 9; ++y)
        {
            for (int x = 0; x < 9; ++x)
            {
                // A 12-unit impulse splits equally between adjacent texels at each half-integer kernel offset.
                const int      dx = std::abs(x - 4), dy = std::abs(y - 4), lowerOffset = horizontal ? 1 : 2;
                const bool     onX = dx == lowerOffset || dx == lowerOffset + 1, onY = dy == lowerOffset || dy == lowerOffset + 1;
                const float    expected = ((dy == 0 && onX) || (dx == 0 && onY) || (onX && onY)) ? 0.5f : 0.0f;
                const uint32_t pixel = 16 + horizontal * 81 + y * 9 + x;
                EXPECT_NEAR(values[pixel * 4], expected, 2e-5) << horizontal << ":" << x << "," << y;
                EXPECT_FLOAT_EQ(values[pixel * 4 + 1], 0);
                EXPECT_FLOAT_EQ(values[pixel * 4 + 2], 0);
                energy += values[pixel * 4];
            }
        }
        EXPECT_NEAR(energy, 12.0, 2e-4);
    }
}

TEST_F(SceneAssetLoaderTest, RendererMaskUsesReferenceDitherAndDistanceCutoffWhileOpaqueIgnoresAlpha)
{
    hz::RenderContext           context({ .pAppName = "RendererMaskAlphaTest", .enableGpuValidation = true });
    const std::filesystem::path shaders =
        std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Examples/Renderer/Shaders");
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, shaders.string().c_str());
    uint32_t normalDDS[36] = {};
    normalDDS[0] = 0x20534444; // DDS magic.
    normalDDS[1] = 124;
    normalDDS[2] = 0x81007;
    normalDDS[3] = normalDDS[4] = 4;
    normalDDS[5] = 16;
    normalDDS[19] = 32;
    normalDDS[20] = 4;
    normalDDS[21] = 0x32495441; // ATI2 / BC5_UNORM.
    normalDDS[27] = 0x1000;
    normalDDS[32] = normalDDS[34] = 0x8080; // Both BC4 blocks decode to 128/255; the texture has no Z channel.
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, directory.string().c_str());
    FileStream normalStream = {};
    ASSERT_TRUE(fsOpenStreamFromPath(RD_OTHER_FILES, "normal-bc5.dds", FM_WRITE, &normalStream));
    const size_t normalBytes = fsWriteToStream(&normalStream, normalDDS, sizeof(normalDDS));
    fsCloseStream(&normalStream);
    ASSERT_EQ(normalBytes, sizeof(normalDDS));
    hz::GPUTexture normalMap = context.loadTexture(RD_OTHER_FILES, "normal-bc5.dds");
    ASSERT_TRUE(normalMap.isValid());
    ASSERT_EQ(normalMap.getFormat(), hz::Format::DXBC5_UNORM);
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, shaders.string().c_str());
    hz::GPUPipeline geometry =
        context.createGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                                                     { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                                                         .sourceDirectory = RD_OTHER_FILES,
                                                         .pFileName = "Geometry.slang" },
                                         .colorTargets = { { .format = hz::Format::R8G8B8A8_UNORM },
                                                           { .format = hz::Format::R8G8B8A8_UNORM },
                                                           { .format = hz::Format::R8G8B8A8_UNORM },
                                                           { .format = hz::Format::R8G8B8A8_UNORM } } });
    fsSetPathForResourceDir(&fileSystem, RM_PROJECT, RD_OTHER_FILES, directory.string().c_str());
    hz::GPUPipeline reader =
        context.createComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP,
                                                                      .pSource = kCookbookTextureReadback,
                                                                      .sourceSize = sizeof(kCookbookTextureReadback) - 1,
                                                                      .pEntryPoint = "CSMain" } } } });
    ASSERT_TRUE(geometry.isValid() && reader.isValid());
    const auto input = [&](const void* data, uint32_t size, uint32_t stride = 0)
    {
        return context.createBuffer({ .size = size,
                                      .elementCount = stride ? size / stride : size / 4,
                                      .structStride = stride,
                                      .pInitialData = data,
                                      .initialDataSize = size,
                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                      .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                      .descriptors = stride ? DESCRIPTOR_TYPE_BUFFER : DESCRIPTOR_TYPE_BUFFER_RAW,
                                      .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    };
    const auto image = [&](hz::Format format)
    {
        return context.createTexture({ .width = 4,
                                       .height = 4,
                                       .format = format,
                                       .startState = RESOURCE_STATE_RENDER_TARGET,
                                       .descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                       .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                       .renderTarget = true });
    };
    hz::GPUTexture alpha = image(hz::Format::R32G32B32A32_SFLOAT), targets[4];
    for (hz::GPUTexture& target : targets)
    {
        target = image(hz::Format::R8G8B8A8_UNORM);
    }
    struct FrameData
    {
        Matrix4 vp, previous, inverse;
        Vector4 eye;
    };
    const FrameData          camera = { Matrix4::identity(), Matrix4::identity(), Matrix4::identity(), Vector4(0, 0, 2, 1) };
    const hz::RenderInstance instance;
    hz::GpuMaterialRecord    material;
    material.baseColorTexture = { 0, 9 };
    material.normalTexture = { 1, 9 };
    material.alphaCutoff = .5f;
    const hz::GpuTextureRecord texture[] = { { .id = { 0, 9 }, .srv = alpha.getSrvIndex() },
                                             { .id = { 1, 9 }, .srv = normalMap.getSrvIndex() } };
    // Production vertex storage: float3 positions, oct-normal uint, and packed half2 UV.
    const float                positions[] = { -1, -1, .5f, 3, -1, .5f, -1, 3, .5f };
    const uint32_t             normals[] = { 0x80008000, 0x80008000, 0x80008000 };
    const uint32_t             nearUV[] = { 0, 0x00003000, 0x30000000 }; // half(.125): fwidth(uv.x)=1/64 in the 4x4 viewport.
    const uint32_t             farUV[] = { 0, 0x00004800, 0x48000000 };  // half(8): fwidth(uv.x)=1, cutoff becomes .5/32.
    hz::GPUBuffer              frame = input(&camera, sizeof(camera), sizeof(camera)), instances = input(&instance, sizeof(instance));
    hz::GPUBuffer              materials = input(&material, sizeof(material)), textures = input(&texture, sizeof(texture));
    hz::GPUBuffer              vertices = input(positions, sizeof(positions)), normal = input(normals, sizeof(normals)),
                  uv = input(nearUV, sizeof(nearUV));
    hz::GPUSampler sampler = context.createSampler();
    hz::GPUBuffer  output = context.createBuffer({ .size = 16 * 16,
                                                   .elementCount = 16,
                                                   .structStride = 16,
                                                   .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                   .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                   .descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer  readback = context.createBuffer({ .size = 2 * 16 * 16,
                                                     .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                     .startState = RESOURCE_STATE_COPY_DEST,
                                                     .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    struct Case
    {
        uint32_t mode;
        float    alpha;
        bool     distant;
        uint32_t coverage;
    };
    const Case cases[] = { { 0, 0, false, 16 }, { 1, 0, false, 0 }, { 1, 1, false, 16 }, { 1, .75f, false, 8 }, { 1, .25f, true, 7 } };
    for (const Case& scenario : cases)
    {
        SCOPED_TRACE(::testing::Message() << scenario.mode << ":" << scenario.alpha << ":" << scenario.distant);
        material.alphaMode = scenario.mode;
        ASSERT_TRUE(context.updateBuffer(materials, 0, &material, sizeof(material)));
        ASSERT_TRUE(context.updateBuffer(uv, 0, scenario.distant ? farUV : nearUV, sizeof(nearUV)));
        hz::CommandList& commands = context.acquireCommandList();
        commands.beginRendering({ .colorAttachments = { { .pTexture = &alpha,
                                                          .loadAction = LOAD_ACTION_CLEAR,
                                                          .storeAction = STORE_ACTION_STORE,
                                                          .clearValue = { .r = 1, .g = 1, .b = 1, .a = scenario.alpha } } } });
        commands.endRendering();
        commands.setPipeline(geometry);
        commands.beginRendering(
            { .colorAttachments = { { .pTexture = &targets[0], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
                                    { .pTexture = &targets[1], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
                                    { .pTexture = &targets[2], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE },
                                    { .pTexture = &targets[3], .loadAction = LOAD_ACTION_CLEAR, .storeAction = STORE_ACTION_STORE } } },
            { .sampledTextures = { &alpha, &normalMap },
              .buffers = { &frame, &instances, &materials, &textures, &vertices, &normal, &uv } });
        commands.setViewport(0, 0, 4, 4);
        commands.setScissor(0, 0, 4, 4);
        uint32_t constants[27] = {};
        constants[3] = frame.getSrvIndex();
        constants[4] = instances.getSrvIndex();
        constants[5] = materials.getSrvIndex();
        constants[6] = textures.getSrvIndex();
        constants[7] = sampler.getIndex();
        constants[8] = vertices.getSrvIndex();
        constants[9] = normal.getSrvIndex();
        constants[10] = uv.getSrvIndex();
        commands.setRootConstant(constants);
        commands.draw(3);
        commands.endRendering();
        commands.setPipeline(reader);
        commands.setRootConstant({ targets[2].getSrvIndex(), output.getUavIndex(), 0 });
        commands.dispatch(1, 1, 1, { .sampledTextures = { &targets[2] }, .storageBuffers = { &output } });
        commands.copyBuffer(readback, 0, output, 0, 16 * 16);
        commands.setRootConstant({ targets[1].getSrvIndex(), output.getUavIndex(), 0 });
        commands.dispatch(1, 1, 1, { .sampledTextures = { &targets[1] }, .storageBuffers = { &output } });
        commands.copyBuffer(readback, 16 * 16, output, 0, 16 * 16);
        context.wait(context.submit(commands));
        const float* pixels = (const float*)readback.get()->pCpuMappedAddress;
        ASSERT_NE(pixels, nullptr);
        uint32_t coverage = 0;
        for (uint32_t i = 0; i < 16; ++i)
        {
            EXPECT_TRUE(pixels[i * 4] == 0.0f || pixels[i * 4] == 1.0f);
            coverage += pixels[i * 4] > .5f ? 1u : 0u;
            if (pixels[i * 4] > .5f)
            {
                // A flat BC5 map must remain +Z (oct-encoded near .5/.5), not flip to -Z because sampled B is zero.
                EXPECT_NEAR(pixels[64 + i * 4], .5f, 1.0f / 255.0f);
                EXPECT_NEAR(pixels[64 + i * 4 + 1], .5f, 1.0f / 255.0f);
            }
        }
        EXPECT_EQ(coverage, scenario.coverage);
    }
}
