#include <gtest/gtest.h>
#include <filesystem>
#include <vector>

#include "Scene/AssetRegistry.h"
#include "Scene/SceneAssetLoader.h"
#include "Scene/SceneAsset.h"
#include "Scene/TextureAsset.h"
#include "Scene/AssetLoaders.h"
#include "Scene/MeshAsset.h"
#include "Scene/MaterialAsset.h"
#include "Scene/WorldAssetBindings.h"
#include "Scene/RenderScene.h"
#include "Scene/SceneFrame.h"
#include "Scene/WorldDocument.h"
#include "Core/IUniquePtr.h"
#include "Resources/IResourceLoader.h"
#include "Graphics/RenderContext.h"
#include "Core/IThread.h"
#include "Core/IContainer.h"
#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"

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
    static void upload(void*, const hz::AssetRegistry& registry, hz::Span<hz::AssetHandle> dependencies, void* pUser)
    {
        ++((TestAssetLoader*)pUser)->uploads;
        for (hz::AssetHandle dependency : dependencies)
            EXPECT_EQ(registry.getState(dependency), hz::AssetState::Ready);
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

class AssetRegistryTest: public ::testing::Test
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

TEST_F(AssetRegistryTest, DeduplicatesRequestsAndRejectsStaleHandlesAfterReuse)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    TestAssetLoader   loader;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle first = registry.request(assetID(1));
    ASSERT_TRUE(first.isValid());
    EXPECT_EQ(registry.request(assetID(1)), first);
    EXPECT_EQ(registry.getState(first), hz::AssetState::Queued);
    registry.update();
    EXPECT_EQ(registry.getState(first), hz::AssetState::Ready);
    EXPECT_EQ(loader.begun, 1u);
    EXPECT_TRUE(registry.release(first));
    registry.update();
    EXPECT_EQ(registry.getState(first), hz::AssetState::Ready);
    EXPECT_TRUE(registry.release(first));
    registry.update();
    EXPECT_EQ(registry.getCount(), 0u);
    const hz::AssetHandle second = registry.request(assetID(1));
    EXPECT_EQ(second.index, first.index);
    EXPECT_NE(second.generation, first.generation);
    EXPECT_FALSE(registry.release(first));
    EXPECT_EQ(registry.getState(first), hz::AssetState::Invalid);
    registry.update();
    registry.shutdown();
    EXPECT_EQ(loader.destroyed.size(), 2u);
}

TEST_F(AssetRegistryTest, SharesDependenciesAndDestroysParentsBeforeChildren)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1), assetID(1) });
    add(catalog, 3, { assetID(1) });
    EXPECT_EQ(catalog.find(assetID(2))->dependencies.count, 1u);
    TestAssetLoader   loader;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle a = registry.request(assetID(2));
    const hz::AssetHandle b = registry.request(assetID(3));
    registry.update();
    EXPECT_EQ(loader.begun, 3u);
    EXPECT_TRUE(registry.release(a));
    registry.update();
    EXPECT_EQ(registry.getCount(), 2u);
    EXPECT_EQ(registry.getState(b), hz::AssetState::Ready);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2 }));
    registry.shutdown();
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2, 3, 1 }));
}

TEST_F(AssetRegistryTest, RejectsInvalidDependencyGraphsWithoutChangingExistingReferences)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1), assetID(99) });
    add(catalog, 3, { assetID(4) });
    add(catalog, 4, { assetID(3) });
    ASSERT_TRUE(
        catalog.add({ .id = assetID(5), .type = hz::AssetType::Texture, .pSourcePath = "test.png", .location = { .pPath = "test.dds" } }));
    TestAssetLoader   loader;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle existing = registry.request(assetID(1));
    hz::AssetError        error = hz::AssetError::None;
    EXPECT_FALSE(registry.request(assetID(2), &error).isValid());
    EXPECT_EQ(error, hz::AssetError::UnknownAsset);
    EXPECT_FALSE(registry.request(assetID(3), &error).isValid());
    EXPECT_EQ(error, hz::AssetError::DependencyCycle);
    EXPECT_FALSE(registry.request(assetID(5), &error).isValid());
    EXPECT_EQ(error, hz::AssetError::MissingLoader);
    EXPECT_EQ(registry.getCount(), 1u);
    EXPECT_TRUE(registry.release(existing));
    registry.update();
    EXPECT_EQ(registry.getCount(), 0u);
    EXPECT_EQ(loader.begun, 0u);
}

TEST_F(AssetRegistryTest, CancellationDrainsLateResultsWithoutPublishingOrReusingTheSlot)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle handle = registry.request(assetID(1));
    registry.update();
    ASSERT_EQ(registry.getState(handle), hz::AssetState::LoadingCPU);
    EXPECT_TRUE(registry.release(handle));
    registry.update();
    EXPECT_EQ(registry.getState(handle), hz::AssetState::Retiring);
    EXPECT_EQ(registry.getScene(handle), nullptr);
    hz::AssetError error;
    EXPECT_FALSE(registry.request(assetID(1), &error).isValid());
    EXPECT_EQ(error, hz::AssetError::Retiring);
    EXPECT_EQ(loader.requestsDestroyed, 0u);
    loader.jobs[1]->ready = true;
    registry.update();
    EXPECT_EQ(registry.getState(handle), hz::AssetState::Invalid);
    EXPECT_EQ(loader.requestsDestroyed, 1u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1 }));
}

TEST_F(AssetRegistryTest, WaitsForDependenciesBeforePublishingDecodedPayload)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1) });
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle handle = registry.request(assetID(2));
    registry.update();
    loader.jobs[2]->ready = true;
    registry.update();
    EXPECT_EQ(registry.getState(handle), hz::AssetState::WaitingDependencies);
    EXPECT_EQ(registry.getScene(handle), nullptr);
    loader.jobs[1]->ready = true;
    registry.update();
    EXPECT_EQ(registry.getState(handle), hz::AssetState::Ready);
    registry.shutdown();
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2, 1 }));
}

TEST_F(AssetRegistryTest, DependencyFailureDrainsPendingParentAndReleasesPartialPayloads)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1) });
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle parent = registry.request(assetID(2));
    registry.update();
    loader.jobs[1]->failed = true;
    loader.jobs[1]->ready = true;
    registry.update();
    EXPECT_EQ(registry.getState(parent), hz::AssetState::Failed);
    EXPECT_EQ(registry.getError(parent), hz::AssetError::DependencyFailed);
    EXPECT_EQ(registry.getScene(parent), nullptr);
    EXPECT_EQ(registry.getCount(), 2u);
    loader.jobs[2]->ready = true;
    registry.update();
    EXPECT_EQ(registry.getCount(), 1u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1, 2 }));
    EXPECT_TRUE(registry.release(parent));
    registry.update();
    EXPECT_EQ(registry.getCount(), 0u);
    loader.failBegin = 1;
    const hz::AssetHandle failed = registry.request(assetID(1));
    registry.update();
    EXPECT_EQ(registry.getState(failed), hz::AssetState::Failed);
    EXPECT_EQ(registry.getError(failed), hz::AssetError::LoadFailed);
}

TEST_F(AssetRegistryTest, ShutdownDrainsPendingJobsAndIsIdempotent)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1) });
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    registry.request(assetID(2));
    registry.update();
    registry.shutdown();
    EXPECT_EQ(loader.requestsDestroyed, 2u);
    EXPECT_EQ(loader.destroyed.size(), 2u);
    EXPECT_EQ(registry.getCount(), 0u);
    hz::AssetError error;
    EXPECT_FALSE(registry.request(assetID(1), &error).isValid());
    EXPECT_EQ(error, hz::AssetError::ShuttingDown);
    registry.shutdown();
    EXPECT_EQ(loader.requestsDestroyed, 2u);
}

TEST_F(AssetRegistryTest, TraversesAndRetiresDeepDependenciesIteratively)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    for (uint32_t i = 2; i <= 1024; ++i)
        add(catalog, i, { assetID(i - 1) });
    TestAssetLoader   loader;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle handle = registry.request(assetID(1024));
    registry.update();
    EXPECT_EQ(registry.getState(handle), hz::AssetState::Ready);
    EXPECT_TRUE(registry.release(handle));
    registry.update();
    EXPECT_EQ(registry.getCount(), 0u);
    ASSERT_EQ(loader.destroyed.size(), 1024u);
    EXPECT_EQ(loader.destroyed.front(), 1024u);
    EXPECT_EQ(loader.destroyed.back(), 1u);
}

TEST_F(AssetRegistryTest, UploadWaitsForDependenciesAndCancellationDrainsBeforeDestruction)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2, { assetID(1) });
    TestAssetLoader   loader;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getGpuLoader());
    const hz::AssetHandle parent = registry.request(assetID(2));
    registry.update();
    EXPECT_EQ(loader.uploads, 1u);
    EXPECT_EQ(registry.getState(parent), hz::AssetState::WaitingDependencies);
    const hz::AssetHandle child = registry.find(assetID(1));
    EXPECT_EQ(registry.getState(child), hz::AssetState::UploadingGPU);
    registry.release(parent);
    registry.update();
    EXPECT_EQ(registry.getState(parent), hz::AssetState::Invalid);
    EXPECT_EQ(registry.getState(child), hz::AssetState::Retiring);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2 }));
    loader.uploadReady = true;
    registry.update();
    EXPECT_EQ(registry.getCount(), 0u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 2, 1 }));
    EXPECT_EQ(loader.uploadWaits, 0u);
}

TEST_F(AssetRegistryTest, UploadFailureAndShutdownUseTheSamePayloadCleanup)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    TestAssetLoader   loader;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getGpuLoader());
    const hz::AssetHandle handle = registry.request(assetID(1));
    registry.update();
    loader.uploadReady = true;
    loader.uploadFailed = true;
    registry.update();
    EXPECT_EQ(registry.getState(handle), hz::AssetState::Failed);
    EXPECT_EQ(registry.getError(handle), hz::AssetError::UploadFailed);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1 }));
    registry.release(handle);
    registry.update();
    loader.uploadReady = false;
    registry.request(assetID(1));
    registry.update();
    registry.shutdown();
    EXPECT_EQ(loader.uploadWaits, 1u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1, 1 }));
}

TEST_F(AssetRegistryTest, SnapshotsPinPayloadsAndKeepTheirOriginalMembership)
{
    hz::AssetCatalog catalog;
    add(catalog, 1);
    add(catalog, 2);
    TestAssetLoader   loader;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle first = registry.request(assetID(1));
    registry.update();
    hz::AssetSnapshot     old = registry.captureSnapshot();
    const hz::SceneAsset* pFirst = old.getScene(first);
    ASSERT_NE(pFirst, nullptr);
    const MemoryTrackingStats before = memGetTrackingStats();
    for (uint32_t i = 0; i < 100; ++i)
    {
        hz::AssetSnapshot frame = registry.captureSnapshot();
        registry.update();
    }
    const MemoryTrackingStats after = memGetTrackingStats();
    if (after.trackingEnabled)
    {
        EXPECT_EQ(after.totalAllocationCount, before.totalAllocationCount);
        EXPECT_EQ(after.reallocationCount, before.reallocationCount);
    }
    registry.release(first);
    registry.update();
    EXPECT_EQ(registry.getState(first), hz::AssetState::Retiring);
    EXPECT_EQ(registry.getScene(first), nullptr);
    EXPECT_EQ(old.getScene(first), pFirst);
    const hz::AssetHandle second = registry.request(assetID(2));
    registry.update();
    hz::AssetSnapshot current = registry.captureSnapshot();
    EXPECT_EQ(current.getScene(first), nullptr);
    EXPECT_EQ(old.getScene(second), nullptr);
    EXPECT_NE(current.getScene(second), nullptr);
    old = {};
    registry.update();
    EXPECT_EQ(registry.getState(first), hz::AssetState::Invalid);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1 }));
    current = {};
    registry.shutdown();
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1, 2 }));
}

static const char kEmptyScene[] = R"json({
    "format":"Horizon.SceneAsset", "version":1,
    "id":"00000000-0000-4000-8000-000000000001",
    "source":"scene.gltf", "contentHash":"fnv1a64:0123456789abcdef", "identityHash":"0123456789abcdef",
    "nodes":[], "meshes":[], "materials":[], "textures":[]
})json";

class SceneAssetLoaderTest: public AssetRegistryTest
{
protected:
    void SetUp() override
    {
        AssetRegistryTest::SetUp();
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
        AssetRegistryTest::TearDown();
    }
    static void wait(hz::AssetRegistry& registry, hz::AssetHandle handle)
    {
        for (uint32_t i = 0; i < 5000; ++i)
        {
            registry.update();
            if (registry.getState(handle) == hz::AssetState::Ready || registry.getState(handle) == hz::AssetState::Failed)
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
    hz::AssetRegistry    registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle handle = registry.request(scene.getID());
    EXPECT_EQ(registry.getScene(handle), nullptr);
    wait(registry, handle);
    ASSERT_EQ(registry.getState(handle), hz::AssetState::Ready);
    ASSERT_NE(registry.getScene(handle), nullptr);
    EXPECT_EQ(registry.getScene(handle)->getID(), scene.getID());
    registry.release(handle);
    registry.update();
    EXPECT_EQ(registry.getScene(handle), nullptr);
}

TEST_F(SceneAssetLoaderTest, RejectsMissingFileAndMismatchedIdentityAndDrainsOnShutdown)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.add({ .id = assetID(1), .pSourcePath = "scene.gltf", .location = { .pPath = "missing.sceneasset.json" } }));
    ASSERT_TRUE(catalog.add({ .id = assetID(2), .pSourcePath = "scene.gltf", .location = { .pPath = "scene.sceneasset.json" } }));
    hz::SceneAssetLoader loader(RD_OTHER_FILES);
    hz::AssetRegistry    registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    const hz::AssetHandle missing = registry.request(assetID(1));
    const hz::AssetHandle mismatched = registry.request(assetID(2));
    wait(registry, missing);
    wait(registry, mismatched);
    EXPECT_EQ(registry.getError(missing), hz::AssetError::LoadFailed);
    EXPECT_EQ(registry.getError(mismatched), hz::AssetError::LoadFailed);
    registry.release(mismatched);
    registry.update();
    registry.request(assetID(2));
    registry.update();
    registry.shutdown();
    EXPECT_EQ(registry.getCount(), 0u);
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
    hz::AssetRegistry    registry(catalog);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    registry.setLoader(hz::AssetType::Mesh, meshLoader.getLoader());
    const hz::AssetHandle handle = registry.request(scene.getID());
    wait(registry, handle);
    EXPECT_EQ(registry.getState(handle), hz::AssetState::Failed);
    EXPECT_EQ(registry.getScene(handle), nullptr);
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
    hz::AssetRegistry      registry(catalog);
    registry.setLoader(hz::AssetType::Texture, loader.getLoader());
    const hz::AssetHandle handle = registry.request(assetID(1));
    EXPECT_EQ(registry.request(assetID(1)), handle);
    registry.release(handle);
    wait(registry, handle);
    ASSERT_EQ(registry.getState(handle), hz::AssetState::Ready);
    hz::AssetSnapshot     snapshot = registry.captureSnapshot();
    const hz::GPUTexture& texture = snapshot.getTexture(handle)->getTexture();
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
    const hz::SubmitHandle submission = context.submit(commands);
    registry.release(handle);
    registry.retireSnapshot(std::move(snapshot), context, submission);
    EXPECT_FALSE(snapshot.isValid());
    EXPECT_EQ(registry.getState(handle), hz::AssetState::Retiring);
    registry.shutdown();
    EXPECT_TRUE(context.isComplete(submission));
    EXPECT_EQ(registry.getCount(), 0u);
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
    static_assert(sizeof(hz::RenderInstance) == 192 && offsetof(hz::RenderInstance, current) == 32 &&
                  offsetof(hz::RenderInstance, previous) == 96);
    static_assert(sizeof(hz::GpuMaterialRecord) == 88 && offsetof(hz::GpuMaterialRecord, baseColorFactor) == 40 &&
                  offsetof(hz::GpuMaterialRecord, alphaMode) == 80);
    struct FrameData
    {
        Matrix4 viewProjection;
        Matrix4 previousViewProjection;
        Matrix4 inverseViewProjection;
        Vector4 eye;
    };
    static_assert(sizeof(FrameData) == 208 && offsetof(FrameData, eye) == 192);
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
    hz::AssetRegistry      registry(catalog);
    registry.setLoader(hz::AssetType::Texture, loader.getLoader());
    const hz::AssetHandle canceled = registry.request(assetID(1));
    registry.update();
    registry.release(canceled);
    for (uint32_t i = 2; i <= 5; ++i)
    {
        const hz::AssetHandle invalid = registry.request(assetID(i));
        wait(registry, invalid);
        EXPECT_EQ(registry.getError(invalid), hz::AssetError::UploadFailed);
        EXPECT_EQ(registry.getTexture(invalid), nullptr);
    }
    registry.shutdown();
    EXPECT_EQ(registry.getCount(), 0u);
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
    hz::AssetRegistry registry(catalog);
    loaders.registerLoaders(registry);
    const hz::AssetHandle root = registry.request(scene.getID());
    const hz::AssetHandle second = registry.request(scene.getMeshes().pData[1].id);
    wait(registry, root);
    ASSERT_EQ(registry.getState(root), hz::AssetState::Ready);
    EXPECT_EQ(registry.getCount(), 5u);
    const hz::AssetHandle first = registry.find(scene.getMeshes().pData[0].id);
    const hz::MeshAsset*  pFirst = registry.getMesh(first);
    const hz::MeshAsset*  pSecond = registry.getMesh(second);
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
    const hz::MaterialAsset* pMaterial = registry.getMaterial(registry.find(scene.getMaterials().pData[0].id));
    ASSERT_NE(pMaterial, nullptr);
    EXPECT_FLOAT_EQ(pMaterial->getDescription().roughnessFactor, 0.8f);
    ASSERT_NE(registry.getTexture(registry.find(pMaterial->getDescription().baseColorTexture)), nullptr);
    hz::AssetSnapshot snapshot = registry.captureSnapshot();
    hz::GPUBuffer     readback = context.createBuffer({ .size = 36,
                                                        .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                        .startState = RESOURCE_STATE_COPY_DEST,
                                                        .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::CommandList&  commands = context.acquireCommandList();
    commands.copyBuffer(readback, 0, pFirst->getVertexBuffer(0), 0, 36);
    const hz::SubmitHandle submission = context.submit(commands);
    registry.release(root);
    registry.release(second);
    EXPECT_EQ(snapshot.getMesh(first), pFirst);
    registry.retireSnapshot(std::move(snapshot), context, submission);
    registry.shutdown();
    EXPECT_EQ(registry.getCount(), 0u);
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
    hz::AssetRegistry registry(catalog);
    loaders.registerLoaders(registry);
    const hz::AssetHandle handle = registry.request(asset.getMeshes().pData[0].id);
    wait(registry, handle);
    ASSERT_EQ(registry.getState(handle), hz::AssetState::Ready);
    const hz::MeshAsset* mesh = registry.getMesh(handle);
    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->getIndexType(), INDEX_TYPE_UINT32);
    const hz::MeshBounds& bounds = mesh->getBounds(0);
    const float           minimum[] = { -1, -5, -2 }, maximum[] = { 4, 2, 6 };
    for (uint32_t axis = 0; axis < 3; ++axis)
    {
        EXPECT_FLOAT_EQ(bounds.minimum[axis], minimum[axis]);
        EXPECT_FLOAT_EQ(bounds.maximum[axis], maximum[axis]);
    }
    hz::AssetSnapshot        snapshot = registry.captureRenderSnapshot();
    const hz::GpuMeshRecord* gpuMesh = snapshot.getGpuTables().getMesh({ .index = handle.index, .generation = handle.generation });
    ASSERT_NE(gpuMesh, nullptr);
    EXPECT_EQ(gpuMesh->indexStride, 4u);
    hz::GPUBuffer    readback = context.createBuffer({ .size = 48,
                                                       .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                       .startState = RESOURCE_STATE_COPY_DEST,
                                                       .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::CommandList& commands = context.acquireCommandList();
    commands.copyBuffer(readback, 0, mesh->getIndexBuffer(), 0, 12);
    commands.copyBuffer(readback, 12, mesh->getVertexBuffer(0), 0, 36);
    const hz::SubmitHandle submission = context.submit(commands);
    registry.release(handle);
    registry.retireSnapshot(std::move(snapshot), context, submission);
    registry.shutdown();
    const uint32_t expectedIndices[] = { 0, 1, 2 };
    const float    expectedPositions[] = { -1, 2, 3, 4, -5, 6, 0, 1, -2 };
    const uint8_t* bytes = (const uint8_t*)readback.get()->pCpuMappedAddress;
    EXPECT_EQ(memcmp(bytes, expectedIndices, sizeof(expectedIndices)), 0);
    EXPECT_EQ(memcmp(bytes + sizeof(expectedIndices), expectedPositions, sizeof(expectedPositions)), 0);
}

TEST_F(AssetRegistryTest, WorldBindingsRetainSharedMeshesAndReleaseRemovedComponents)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(
        catalog.add({ .id = assetID(1), .type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Mesh, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity a = world.createEntity(), b = world.createEntity();
    hz::MeshRenderer renderer = { .mesh = assetID(1) };
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    hz::WorldAssetBindings bindings(world, registry);
    bindings.update();
    ASSERT_EQ(bindings.getCount(), 2u);
    const hz::AssetHandle mesh = bindings.get(a)->mesh;
    EXPECT_EQ(bindings.get(b)->mesh, mesh);
    registry.update();
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
    EXPECT_EQ(registry.getState(mesh), hz::AssetState::LoadingCPU);
    ASSERT_TRUE(world.destroyEntity(b));
    bindings.update();
    EXPECT_EQ(bindings.getCount(), 0u);
    EXPECT_EQ(registry.getState(mesh), hz::AssetState::Retiring);
    loader.jobs[1]->ready = true;
    registry.update();
    EXPECT_EQ(registry.getCount(), 0u);
    EXPECT_EQ(loader.destroyed, (std::vector<uint32_t>{ 1 }));
}

TEST_F(AssetRegistryTest, WorldBindingsSwapMeshesWithoutRetiringSharedAssets)
{
    hz::AssetCatalog catalog;
    for (uint32_t i = 1; i <= 2; ++i)
        ASSERT_TRUE(catalog.add(
            { .id = assetID(i), .type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Mesh, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity a = world.createEntity(), b = world.createEntity();
    hz::MeshRenderer first = { .mesh = assetID(1) }, second = { .mesh = assetID(2) };
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &first));
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &second));
    hz::WorldAssetBindings bindings(world, registry);
    bindings.update();
    const hz::AssetHandle meshA = bindings.get(a)->mesh, meshB = bindings.get(b)->mesh;
    registry.update();
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &second));
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &first));
    bindings.update();
    EXPECT_EQ(bindings.get(a)->mesh, meshB);
    EXPECT_EQ(bindings.get(b)->mesh, meshA);
    EXPECT_EQ(registry.getState(meshA), hz::AssetState::LoadingCPU);
    EXPECT_EQ(registry.getState(meshB), hz::AssetState::LoadingCPU);
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
    registry.update();
    bindings.update();
    EXPECT_TRUE(bindings.get(replacement)->mesh.isValid());
    EXPECT_NE(bindings.get(replacement)->mesh, meshB);
    bindings.clear();
    EXPECT_EQ(bindings.getCount(), 0u);
    EXPECT_EQ(registry.getState(meshA), hz::AssetState::Retiring);
}

TEST_F(AssetRegistryTest, RenderSceneKeepsPendingSlotsAndRejectsReusedGenerations)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(
        catalog.add({ .id = assetID(1), .type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Mesh, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, registry);
    const hz::MeshRenderer renderer = { .mesh = assetID(1) };
    const hz::Entity       a = world.createEntity(), b = world.createEntity();
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    scene.update();
    registry.update();
    const hz::RenderObjectID first = scene.findObject(a), second = scene.findObject(b);
    ASSERT_TRUE(first.isValid());
    ASSERT_TRUE(second.isValid());
    EXPECT_EQ(scene.getActiveObjects().count, 2u);
    EXPECT_EQ(scene.getActiveInstances().count, 2u);
    EXPECT_EQ(scene.getActiveDraws().count, 0u);
    EXPECT_EQ(scene.getObject(first)->instanceCount, 0u);
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
    EXPECT_EQ(scene.getChanges().objects.ranges.count, 0u);
    EXPECT_EQ(scene.getChanges().instances.ranges.count, 0u);
    ASSERT_TRUE(world.removeComponent(a, hz::SceneTypes::meshRenderer));
    scene.update();
    EXPECT_EQ(scene.getObject(first), nullptr);
    EXPECT_EQ(scene.getInstance(instance), nullptr);
    EXPECT_FALSE(scene.findObject(a).isValid());
    EXPECT_EQ(scene.findObject(b), second);
    ASSERT_EQ(scene.getChanges().objects.removed.count, 1u);
    EXPECT_EQ(scene.getChanges().objects.removed.pData[0], first.index);
    EXPECT_TRUE(scene.getChanges().objects.activeList);
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    scene.update();
    const hz::RenderObjectID replacement = scene.findObject(a);
    EXPECT_EQ(replacement.index, first.index);
    EXPECT_NE(replacement.generation, first.generation);
    EXPECT_EQ(scene.getObject(first), nullptr);
    EXPECT_EQ(scene.getInstance(instance), nullptr);
    ASSERT_TRUE(world.destroyEntity(b));
    scene.update();
    EXPECT_EQ(scene.getObject(second), nullptr);
    EXPECT_EQ(scene.getActiveObjects().count, 1u);
    EXPECT_EQ(scene.getObjects().count, 2u);
}

TEST_F(AssetRegistryTest, RenderSceneMergesSparseTransformChangesWithoutAllocating)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(
        catalog.add({ .id = assetID(1), .type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Mesh, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, registry);
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
    registry.update();
    world.commitTransforms();
    scene.update();
    const MemoryTrackingStats before = memGetTrackingStats();
    for (hz::Entity entity : entities)
        if ((scene.findObject(entity).index % 2) == 0)
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
    EXPECT_EQ(scene.getChanges().objects.ranges.count, 0u);
    EXPECT_EQ(scene.getChanges().instanceIndices.count, 0u);
}

TEST_F(AssetRegistryTest, RenderSceneJournalCatchesUpAndFallsBackAfterOverflow)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(
        catalog.add({ .id = assetID(1), .type = hz::AssetType::Mesh, .pSourcePath = "scene.gltf", .location = { .pPath = "scene.bin" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Mesh, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, registry, { .journalCapacity = 8 });
    const hz::Entity       entity = world.createEntity();
    const hz::MeshRenderer renderer = { .mesh = assetID(1) };
    hz::LocalTransform     transform;
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
    scene.update();
    const uint64_t first = scene.getRevision();
    EXPECT_TRUE(scene.getChangesSince(0).full);
    for (uint32_t i = 1; i <= 4; ++i)
    {
        transform.translation = Vector3((float)i, 0, 0);
        ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
        scene.update();
    }
    const hz::RenderSceneSync delta = scene.getChangesSince(first);
    EXPECT_FALSE(delta.full);
    ASSERT_EQ(delta.instances.count, 1u);
    EXPECT_EQ(delta.instances.pData[0].first, 0u);
    EXPECT_EQ(delta.instances.pData[0].count, 1u);
    EXPECT_EQ(delta.objects.count, 0u);
    const uint64_t intermediate = scene.getRevision();
    for (uint32_t i = 5; i <= 12; ++i)
    {
        transform.translation = Vector3((float)i, 0, 0);
        ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
        scene.update();
    }
    EXPECT_TRUE(scene.getChangesSince(first).full);
    EXPECT_FALSE(scene.getChangesSince(intermediate).full);
    EXPECT_EQ(scene.getChangesSince(scene.getRevision()).instances.count, 0u);
    const uint64_t beforeRemove = scene.getRevision();
    ASSERT_TRUE(world.removeComponent(entity, hz::SceneTypes::meshRenderer));
    scene.update();
    const hz::RenderSceneSync removal = scene.getChangesSince(beforeRemove);
    EXPECT_FALSE(removal.full);
    EXPECT_TRUE(removal.activeObjects);
    EXPECT_TRUE(removal.activeInstances);
    EXPECT_FALSE(removal.activeDraws);
    EXPECT_EQ(removal.objects.count, 1u);
    EXPECT_EQ(removal.instances.count, 1u);
}

TEST_F(AssetRegistryTest, RenderSceneExtractsLightsAndCamerasWithStableSlotsAndNoFrameAllocation)
{
    hz::AssetCatalog  catalog;
    hz::AssetRegistry registry(catalog);
    hz::TypeRegistry  types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World       world(types);
    hz::RenderScene scene(world, registry, { .journalCapacity = 32 });
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
        entities[i] = world.createObject({ .high = 2, .low = i + 1 });
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
        EXPECT_EQ(extracted->objectID, world.getObjectID(entities[i]));
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
        EXPECT_EQ(camera->objectID, extracted->objectID);
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
    // Change every other entity, then merge nonadjacent camera/light ranges in the journal.
    transform.translation = Vector3(2, 3, 4);
    lens.verticalFov = 0.6f;
    for (uint32_t i = 0; i < 9; i += 2)
    {
        ASSERT_TRUE(world.setComponent(entities[i], hz::SceneTypes::localTransform, &transform));
        ASSERT_TRUE(world.setComponent(entities[i], hz::SceneTypes::camera, &lens));
    }
    scene.update();
    const hz::RenderSceneSync changes = scene.getChangesSince(initial);
    EXPECT_FALSE(changes.full);
    EXPECT_EQ(changes.lights.count, 5u);
    EXPECT_EQ(changes.cameras.count, 5u);
    EXPECT_FALSE(changes.activeLights);
    EXPECT_FALSE(changes.activeCameras);
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

TEST_F(AssetRegistryTest, RenderSceneEmissionBindingsHandlePendingFailureAndLeaseHandoffs)
{
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.add(
        { .id = assetID(1), .type = hz::AssetType::Texture, .pSourcePath = "light.png", .location = { .pPath = "light.dds" } }));
    TestAssetLoader loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Texture, loader.getLoader());
    add(catalog, 2);
    registry.setLoader(hz::AssetType::Scene, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    hz::RenderScene  scene(world, registry);
    const hz::Entity a = world.createEntity(), b = world.createEntity();
    hz::Light        light = { .type = hz::LightType::Dome, .texture = assetID(1) };
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::light, &light));
    scene.update();
    const hz::RenderLightID id = scene.findLight(a);
    const hz::AssetHandle   handle = registry.find(light.texture);
    ASSERT_TRUE(handle.isValid());
    registry.update();
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
    registry.update();
    EXPECT_EQ(registry.find(light.texture), handle);
    EXPECT_EQ(loader.begun, 1u);
    loader.jobs[1]->failed = true;
    loader.jobs[1]->ready = true;
    registry.update();
    scene.update();
    EXPECT_EQ(scene.getLight(scene.findLight(b))->ready, 0u);
    for (uint32_t i = 0; i < 20; ++i)
    {
        registry.update();
        scene.update();
    }
    EXPECT_EQ(loader.begun, 1u);
    light.texture = assetID(999);
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::light, &light));
    scene.update();
    registry.update();
    EXPECT_FALSE(registry.find(assetID(1)).isValid());
    EXPECT_EQ(scene.getLight(scene.findLight(b))->ready, 0u);
    scene.update();
    light.texture = {};
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::light, &light));
    scene.update();
    EXPECT_EQ(scene.getLight(scene.findLight(b))->ready, 1u);
    EXPECT_EQ(registry.getCount(), 0u);
    // A Ready payload of another type must never become a GPU texture reference.
    loader.ready = true;
    light.texture = assetID(2);
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::light, &light));
    scene.update();
    registry.update();
    scene.update();
    EXPECT_EQ(registry.getState(registry.find(light.texture)), hz::AssetState::Ready);
    EXPECT_EQ(scene.getLight(scene.findLight(b))->ready, 0u);
    EXPECT_FALSE(scene.getLight(scene.findLight(b))->texture.isValid());
}

TEST_F(AssetRegistryTest, WorldBindingsKeepFailedRequestsIdleUntilRebuilt)
{
    hz::AssetCatalog catalog;
    TestAssetLoader  loader;
    loader.ready = false;
    hz::AssetRegistry registry(catalog);
    registry.setLoader(hz::AssetType::Mesh, loader.getLoader());
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World              world(types);
    const hz::Entity       entity = world.createEntity();
    const hz::MeshRenderer renderer = { .mesh = assetID(1) };
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::meshRenderer, &renderer));
    hz::WorldAssetBindings bindings(world, registry);
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
    EXPECT_EQ(registry.getCount(), 0u);
    bindings.clear();
    bindings.update();
    EXPECT_EQ(bindings.get(entity)->state, hz::AssetBindingState::Pending);
    EXPECT_TRUE(bindings.get(entity)->mesh.isValid());
    EXPECT_EQ(registry.getCount(), 1u);
}

TEST_F(TypedAssetLiveTest, WorldBindingsResolveSubmeshMaterialsAndKeepSnapshotsAlive)
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
    hz::AssetRegistry registry(catalog);
    loaders.registerLoaders(registry);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity a = world.createEntity(), b = world.createEntity();
    hz::MeshRenderer renderer = { .mesh = scene.getMeshes().pData[0].id };
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    renderer.overrides.pushBack({ .submesh = UINT64_C(0x8000000000000003), .material = alternate });
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    hz::WorldAssetBindings bindings(world, registry);
    bindings.update();
    const hz::AssetHandle mesh = bindings.get(a)->mesh;
    EXPECT_EQ(bindings.get(a)->state, hz::AssetBindingState::Pending);
    wait(registry, mesh);
    bindings.update();
    const hz::AssetHandle overrideMaterial = registry.find(alternate);
    ASSERT_TRUE(overrideMaterial.isValid());
    wait(registry, overrideMaterial);
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

    hz::AssetSnapshot snapshot = registry.captureSnapshot();
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
    EXPECT_EQ(registry.getState(overrideMaterial), hz::AssetState::Retiring);
    EXPECT_NE(snapshot.getMaterial(overrideMaterial), nullptr);

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
    registry.update();
    EXPECT_EQ(bindings.getCount(), 0u);
    EXPECT_EQ(registry.getState(mesh), hz::AssetState::Retiring);
    EXPECT_NE(snapshot.getMesh(mesh), nullptr);
    EXPECT_NE(snapshot.getMaterial(overrideMaterial), nullptr);
    snapshot = {};
    registry.update();
    EXPECT_EQ(registry.getCount(), 0u);
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
    hz::AssetRegistry registry(catalog);
    loaders.registerLoaders(registry);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World          world(types);
    hz::RenderScene    scene(world, registry);
    const hz::ObjectID identity = { .high = 2, .low = 1 };
    const hz::Entity   a = world.createObject(identity), b = world.createEntity();
    hz::MeshRenderer   renderer = { .mesh = assets.getMeshes().pData[0].id };
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    hz::LocalMatrix local;
    local.value = Matrix4::scale(Vector3(-2, 3, 0.5f));
    local.value.setElem(1, 0, 1.0f); // Shear X by Y.
    local.value.setTranslation(Vector3(10, 20, 30));
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::localMatrix, &local));
    scene.update();
    const hz::RenderObjectID object = scene.findObject(a);
    wait(registry, registry.find(renderer.mesh));
    scene.update();
    ASSERT_EQ(scene.getObject(object)->instanceCount, 1u);
    EXPECT_EQ(scene.findObject(a), object);
    ASSERT_EQ(scene.getActiveDraws().count, 4u);
    ASSERT_EQ(scene.getChanges().draws.created.count, 4u);
    const uint32_t             instanceIndex = scene.getInstanceIndices().pData[scene.getObject(object)->instanceOffset];
    const hz::RenderInstanceID instanceID = scene.getInstances().pData[instanceIndex].id;
    const hz::RenderInstance*  instance = scene.getInstance(instanceID);
    ASSERT_NE(instance, nullptr);
    EXPECT_EQ(instance->objectID, identity);
    EXPECT_EQ(instance->object, object);
    EXPECT_FLOAT_EQ(instance->bounds.minimum[0], -3);
    EXPECT_FLOAT_EQ(instance->bounds.maximum[0], 14);
    EXPECT_FLOAT_EQ(instance->bounds.minimum[1], 5);
    EXPECT_FLOAT_EQ(instance->bounds.maximum[1], 26);
    EXPECT_FLOAT_EQ(instance->bounds.minimum[2], 29);
    EXPECT_FLOAT_EQ(instance->bounds.maximum[2], 33);
    hz::Array<hz::RenderDrawID> drawIDs;
    for (uint32_t index : scene.getActiveDraws())
        if (scene.getDraws().pData[index].object == object)
            drawIDs.pushBack(scene.getDraws().pData[index].id);
    ASSERT_EQ(drawIDs.size(), 2u);
    EXPECT_EQ(scene.getDraw(drawIDs[0])->submesh, UINT64_C(0x8000000000000001));
    EXPECT_EQ(scene.getDraw(drawIDs[1])->submesh, UINT64_C(0x8000000000000003));
    const hz::GpuMaterialID original = scene.getDraw(drawIDs[0])->material;

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
    EXPECT_EQ(scene.getChanges().draws.ranges.count, 0u);
    local.value.setTranslation(Vector3(11, 20, 30));
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::localMatrix, &local));
    scene.update();
    EXPECT_FLOAT_EQ((float)scene.getInstance(instanceID)->current.getElem(3, 0), 11);
    EXPECT_FLOAT_EQ((float)scene.getInstance(instanceID)->previous.getElem(3, 0), 11);
    EXPECT_EQ(scene.getChanges().objects.ranges.count, 0u);
    EXPECT_EQ(scene.getChanges().draws.ranges.count, 0u);
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
    EXPECT_EQ(scene.getObject(object)->instanceCount, 0u);
    EXPECT_NE(scene.getDraw(drawIDs[0]), nullptr);
    EXPECT_NE(scene.getDraw(drawIDs[1]), nullptr);
    EXPECT_FALSE(scene.getDraw(drawIDs[1])->material.isValid());
    wait(registry, registry.find(alternate));
    scene.update();
    EXPECT_EQ(scene.getObject(object)->instanceCount, 1u);
    EXPECT_EQ(scene.getDraw(drawIDs[0])->material, original);
    const hz::AssetHandle alternateHandle = registry.find(alternate);
    EXPECT_EQ(scene.getDraw(drawIDs[1])->material,
              (hz::GpuMaterialID{ .index = alternateHandle.index, .generation = alternateHandle.generation }));
    EXPECT_EQ(scene.getChanges().draws.created.count, 0u);
    renderer.overrides.clear();
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    scene.update();
    EXPECT_EQ(scene.getDraw(drawIDs[1])->material, original);
    EXPECT_EQ(scene.getChanges().objects.ranges.count, 0u);
    ASSERT_EQ(scene.getChanges().draws.ranges.count, 1u);
    EXPECT_EQ(scene.getChanges().draws.ranges.pData[0].first, drawIDs[1].index);
    EXPECT_EQ(scene.getChanges().draws.ranges.pData[0].count, 1u);

    ASSERT_TRUE(world.destroyEntity(a));
    scene.update();
    EXPECT_EQ(scene.getObject(object), nullptr);
    EXPECT_EQ(scene.getInstance(instanceID), nullptr);
    EXPECT_EQ(scene.getDraw(drawIDs[0]), nullptr);
    EXPECT_EQ(scene.getDraw(drawIDs[1]), nullptr);
    EXPECT_EQ(scene.getInstanceIndices().pData[object.index], UINT32_MAX);
    EXPECT_EQ(scene.getActiveDraws().count, 2u);
    const hz::RenderObjectID survivor = scene.findObject(b);
    const uint32_t           survivingInstance = scene.getInstanceIndices().pData[scene.getObject(survivor)->instanceOffset];
    EXPECT_EQ(scene.getInstances().pData[survivingInstance].object, survivor);
    const hz::Entity replacement = world.createObject(identity);
    ASSERT_TRUE(world.setComponent(replacement, hz::SceneTypes::meshRenderer, &renderer));
    scene.update();
    const hz::RenderObjectID reused = scene.findObject(replacement);
    EXPECT_EQ(reused.index, object.index);
    EXPECT_NE(reused.generation, object.generation);
    EXPECT_EQ(scene.getActiveDraws().count, 4u);
    for (hz::RenderDrawID old : drawIDs)
    {
        EXPECT_EQ(scene.getDraw(old), nullptr);
        EXPECT_NE(scene.getDraws().pData[old.index].id.generation, old.generation);
    }
}

TEST_F(TypedAssetLiveTest, SceneFramesPairGpuTablesHistoryAndRotatingBackingStorage)
{
    writeTexture();
    writeScene();
    writeGeometry();
    hz::SceneAsset assets;
    ASSERT_TRUE(assets.parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(assets, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "SceneFrameTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = RD_OTHER_FILES });
    hz::AssetRegistry registry(catalog);
    loaders.registerLoaders(registry);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, registry, { .journalCapacity = 8 });
    const hz::Entity       a = world.createEntity(), b = world.createEntity();
    const hz::MeshRenderer renderer = { .mesh = assets.getMeshes().pData[0].id };
    hz::LocalTransform     transform = { .translation = Vector3(1, 0, 0) };
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.setComponent(b, hz::SceneTypes::meshRenderer, &renderer));
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::localTransform, &transform));
    scene.update();
    const hz::AssetHandle meshHandle = registry.find(renderer.mesh);
    wait(registry, meshHandle);
    scene.update();
    const hz::RenderObjectID object = scene.findObject(a);
    const hz::GpuMeshID      meshID = scene.getObject(object)->mesh;
    const uint32_t           instanceIndex = scene.getInstanceIndices().pData[scene.getObject(object)->instanceOffset];
    hz::RenderDrawID         drawID;
    for (uint32_t index : scene.getActiveDraws())
        if (scene.getDraws().pData[index].object == object)
            drawID = scene.getDraws().pData[index].id;
    ASSERT_TRUE(drawID.isValid());
    const hz::GpuMaterialID materialID = scene.getDraw(drawID)->material;
    hz::SceneFramePool      pool(scene, context);
    for (uint32_t i = 0; i < 3; ++i)
    {
        hz::SceneFrameSnapshot warm = pool.capture();
        ASSERT_TRUE(warm.isValid());
        EXPECT_EQ(warm.getBackingIndex(), i);
        const hz::GpuAssetTableSnapshot tables = warm.getAssets().getGpuTables();
        const hz::GpuMeshRecord*        mesh = tables.getMesh(meshID);
        ASSERT_NE(mesh, nullptr);
        EXPECT_EQ(mesh->indexStride, 2u);
        EXPECT_EQ(mesh->submeshCount, 1u);
        EXPECT_EQ(tables.vertexStreams.pData[mesh->vertexStreamOffset].stride, 12u);
        EXPECT_EQ(tables.submeshes.pData[mesh->submeshOffset].indexCount, 3u);
        EXPECT_EQ(mesh->indexSrv, registry.getMesh(meshHandle)->getIndexBuffer().getSrvIndex());
        const hz::GpuMaterialRecord* material = tables.getMaterial(materialID);
        ASSERT_NE(material, nullptr);
        EXPECT_FLOAT_EQ(material->roughnessFactor, 0.8f);
        const hz::GpuTextureRecord* texture = tables.getTexture(material->baseColorTexture);
        ASSERT_NE(texture, nullptr);
        EXPECT_EQ(texture->srv, registry.getTexture(registry.find(assets.getTextures().pData[0].id))->getTexture().getSrvIndex());
        EXPECT_EQ(tables.getTexture({ .index = meshID.index, .generation = meshID.generation }), nullptr);
        EXPECT_TRUE(warm.getBuffers().instances.isValid());
        EXPECT_TRUE(warm.getBuffers().meshes.isValid());
    }
    hz::GPUBuffer          readback = context.createBuffer({ .size = sizeof(hz::RenderInstance),
                                                             .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                             .startState = RESOURCE_STATE_COPY_DEST,
                                                             .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::SceneFrameSnapshot frame = pool.capture();
    const uint64_t         initialRevision = frame.getRevision();
    const uint64_t         initialAssetRevision = frame.getAssets().getRevision();
    transform.translation = Vector3(2, 0, 0);
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::localTransform, &transform));
    scene.update();
    EXPECT_FLOAT_EQ((float)frame.getInstances().pData[instanceIndex].current.getElem(3, 0), 1);
    hz::CommandList& firstCommands = context.acquireCommandList();
    firstCommands.copyBuffer(readback, 0, frame.getBuffers().instances, sizeof(hz::RenderInstance) * instanceIndex,
                             sizeof(hz::RenderInstance));
    const hz::SubmitHandle firstSubmission = context.submit(firstCommands);
    pool.retire(std::move(frame), firstSubmission);
    EXPECT_FALSE(frame.isValid());
    frame = pool.capture();
    EXPECT_GT(frame.getRevision(), initialRevision);
    EXPECT_EQ(frame.getAssets().getRevision(), initialAssetRevision);
    EXPECT_FLOAT_EQ((float)frame.getInstances().pData[instanceIndex].current.getElem(3, 0), 2);
    EXPECT_FLOAT_EQ((float)frame.getInstances().pData[instanceIndex].previous.getElem(3, 0), 1);
    frame = {};
    transform.translation = Vector3(3, 0, 0);
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::localTransform, &transform));
    frame = pool.capture();
    EXPECT_FLOAT_EQ((float)frame.getInstances().pData[instanceIndex].previous.getElem(3, 0), 1);
    frame = {};
    context.wait(firstSubmission);
    const hz::RenderInstance* copied = (const hz::RenderInstance*)readback.get()->pCpuMappedAddress;
    EXPECT_FLOAT_EQ((float)copied->current.getElem(3, 0), 1);
    frame = pool.capture();
    hz::CommandList& movingCommands = context.acquireCommandList();
    movingCommands.copyBuffer(readback, 0, frame.getBuffers().instances, sizeof(hz::RenderInstance) * instanceIndex,
                              sizeof(hz::RenderInstance));
    const hz::SubmitHandle movingSubmission = context.submit(movingCommands);
    pool.retire(std::move(frame), movingSubmission);
    context.wait(movingSubmission);
    pool.collect();
    // History must update even when current stays unchanged, including on every backing.
    for (uint32_t i = 0; i < 3; ++i)
    {
        frame = pool.capture();
        EXPECT_FLOAT_EQ((float)frame.getInstances().pData[instanceIndex].current.getElem(3, 0), 3);
        EXPECT_FLOAT_EQ((float)frame.getInstances().pData[instanceIndex].previous.getElem(3, 0), 3);
        frame = {};
    }
    const MemoryTrackingStats before = memGetTrackingStats();
    for (uint32_t i = 0; i < 30; ++i)
    {
        transform.translation = Vector3((float)i + 4, 0, 0);
        ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::localTransform, &transform));
        frame = pool.capture();
        EXPECT_FLOAT_EQ((float)frame.getInstances().pData[instanceIndex].current.getElem(3, 0), (float)i + 4);
        EXPECT_FLOAT_EQ((float)frame.getInstances().pData[instanceIndex].previous.getElem(3, 0), 3);
        frame = {};
    }
    const MemoryTrackingStats after = memGetTrackingStats();
    if (before.trackingEnabled)
    {
        EXPECT_EQ(before.totalAllocationCount, after.totalAllocationCount);
        EXPECT_EQ(before.reallocationCount, after.reallocationCount);
    }
    const uint64_t backingRevision = scene.getRevision();
    for (uint32_t i = 0; i < 12; ++i)
    {
        transform.translation = Vector3((float)i + 100, 0, 0);
        ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::localTransform, &transform));
        scene.update();
    }
    EXPECT_TRUE(scene.getChangesSince(backingRevision).full);
    for (uint32_t i = 0; i < 3; ++i)
    {
        frame = pool.capture();
        EXPECT_FLOAT_EQ((float)frame.getInstances().pData[instanceIndex].current.getElem(3, 0), 111);
        EXPECT_FLOAT_EQ((float)frame.getInstances().pData[instanceIndex].previous.getElem(3, 0), 3);
        frame = {};
    }
    scene.resetHistory(a);
    frame = pool.capture();
    EXPECT_FLOAT_EQ((float)frame.getInstances().pData[instanceIndex].previous.getElem(3, 0), 111);
    // A reset after capture must survive retiring an older frame.
    transform.translation = Vector3(200, 0, 0);
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::localTransform, &transform));
    scene.resetHistory(a);
    hz::CommandList&       resetCommands = context.acquireCommandList();
    const hz::SubmitHandle resetSubmission = context.submit(resetCommands);
    pool.retire(std::move(frame), resetSubmission);
    context.wait(resetSubmission);
    frame = pool.capture();
    EXPECT_FLOAT_EQ((float)frame.getInstances().pData[instanceIndex].previous.getElem(3, 0), 200);
    frame = {};

    ASSERT_TRUE(world.removeComponent(a, hz::SceneTypes::meshRenderer));
    for (uint32_t i = 0; i < 3; ++i)
    {
        frame = pool.capture();
        EXPECT_EQ(frame.getActiveObjects().count, 1u);
        EXPECT_EQ(frame.getActiveInstances().count, 1u);
        EXPECT_EQ(frame.getActiveDraws().count, 1u);
        EXPECT_FALSE(frame.getObjects().pData[object.index].id.isValid());
        EXPECT_FALSE(frame.getDraws().pData[drawID.index].id.isValid());
        EXPECT_EQ(frame.getInstanceIndices().pData[object.index], UINT32_MAX);
        frame = {};
    }
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    frame = pool.capture();
    const hz::RenderObjectID reused = scene.findObject(a);
    EXPECT_EQ(reused.index, object.index);
    EXPECT_NE(reused.generation, object.generation);
    EXPECT_NE(frame.getDraws().pData[drawID.index].id.generation, drawID.generation);
    const uint32_t reusedInstance = frame.getInstanceIndices().pData[frame.getObjects().pData[reused.index].instanceOffset];
    EXPECT_FLOAT_EQ((float)frame.getInstances().pData[reusedInstance].previous.getElem(3, 0), 200);
    ASSERT_TRUE(world.removeComponent(a, hz::SceneTypes::meshRenderer));
    ASSERT_TRUE(world.removeComponent(b, hz::SceneTypes::meshRenderer));
    scene.update();
    registry.update();
    EXPECT_NE(frame.getAssets().getMesh(meshHandle), nullptr);
    EXPECT_NE(frame.getAssets().getGpuTables().getMesh(meshID), nullptr);
    hz::GPUBuffer    meshReadback = context.createBuffer({ .size = sizeof(hz::GpuMeshRecord),
                                                           .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                           .startState = RESOURCE_STATE_COPY_DEST,
                                                           .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::CommandList& assetCommands = context.acquireCommandList();
    assetCommands.copyBuffer(meshReadback, 0, frame.getBuffers().meshes, sizeof(hz::GpuMeshRecord) * meshID.index,
                             sizeof(hz::GpuMeshRecord));
    const hz::SubmitHandle assetSubmission = context.submit(assetCommands);
    pool.retire(std::move(frame), assetSubmission);
    frame = pool.capture();
    EXPECT_EQ(frame.getActiveObjects().count, 0u);
    EXPECT_EQ(frame.getAssets().getGpuTables().getMesh(meshID), nullptr);
    frame = {};
    context.wait(assetSubmission);
    const hz::GpuMeshRecord* copiedMesh = (const hz::GpuMeshRecord*)meshReadback.get()->pCpuMappedAddress;
    EXPECT_EQ(copiedMesh->id, meshID);
    EXPECT_EQ(copiedMesh->submeshCount, 1u);
    pool.collect();
    registry.update();
    EXPECT_EQ(registry.getCount(), 0u);
    ASSERT_TRUE(world.setComponent(a, hz::SceneTypes::meshRenderer, &renderer));
    scene.update();
    const hz::AssetHandle reloaded = registry.find(renderer.mesh);
    wait(registry, reloaded);
    frame = pool.capture();
    EXPECT_NE(frame.getObjects().pData[scene.findObject(a).index].mesh, meshID);
    EXPECT_EQ(frame.getAssets().getGpuTables().getMesh(meshID), nullptr);
    EXPECT_EQ(frame.getActiveDraws().count, 1u);
}

TEST_F(TypedAssetLiveTest, SceneFramesFreezeLightsAndCameraHistoryAndRetireEmissionTextures)
{
    writeTexture();
    writeScene();
    writeGeometry();
    hz::SceneAsset assets;
    ASSERT_TRUE(assets.parse(kTypedScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(assets, "scene.sceneasset.json"));
    hz::RenderContext context({ .pAppName = "LightCameraFrameTest", .enableGpuValidation = true });
    hz::AssetLoaders  loaders(context, { .vertexLayout = kAssetTestLayout, .cookedDirectory = RD_OTHER_FILES });
    hz::AssetRegistry registry(catalog);
    loaders.registerLoaders(registry);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World              world(types);
    hz::RenderScene        scene(world, registry, { .journalCapacity = 8 });
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
    const hz::AssetHandle    textureHandle = registry.find(light.texture);
    EXPECT_EQ(scene.getLight(lightID)->ready, 0u);
    wait(registry, textureHandle);
    scene.update();
    EXPECT_EQ(scene.getLight(lightID)->ready, 0u); // Mesh emitter still has no mesh.
    EXPECT_TRUE(scene.getLight(lightID)->texture.isValid());
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::meshRenderer, &renderer));
    scene.update();
    wait(registry, registry.find(renderer.mesh));
    scene.update();
    const hz::RenderObjectID objectID = scene.findObject(entity);
    ASSERT_EQ(scene.getLight(lightID)->ready, 1u);
    EXPECT_EQ(scene.getLight(lightID)->object, objectID);
    hz::SceneFramePool pool(scene, context);
    pool.reserve(1, 1, 1, 1);
    for (uint32_t i = 0; i < 3; ++i)
    {
        hz::SceneFrameSnapshot warm = pool.capture();
        EXPECT_EQ(warm.getActiveLights().count, 1u);
        EXPECT_EQ(warm.getActiveCameras().count, 1u);
        EXPECT_TRUE(warm.getBuffers().lights.isValid());
        EXPECT_TRUE(warm.getBuffers().cameras.isValid());
        EXPECT_TRUE(warm.getBuffers().activeLights.isValid());
        EXPECT_TRUE(warm.getBuffers().activeCameras.isValid());
        const hz::RenderLight& frozen = warm.getLights().pData[lightID.index];
        EXPECT_NE(warm.getAssets().getGpuTables().getTexture(frozen.texture), nullptr);
    }
    hz::GPUBuffer          readback = context.createBuffer({ .size = sizeof(hz::RenderCamera) + sizeof(hz::RenderLight),
                                                             .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                             .startState = RESOURCE_STATE_COPY_DEST,
                                                             .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    hz::SceneFrameSnapshot frame = pool.capture();
    transform.translation = Vector3(2, 0, 0);
    lens.verticalFov = 0.6f;
    light.intensity = 8;
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::camera, &lens));
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::light, &light));
    scene.update();
    EXPECT_FLOAT_EQ(frame.getCameras().pData[cameraID.index].lens.verticalFov, 0.8f);
    EXPECT_FLOAT_EQ(frame.getLights().pData[lightID.index].intensity, 4);
    hz::CommandList& commands = context.acquireCommandList();
    commands.copyBuffer(readback, 0, frame.getBuffers().cameras, sizeof(hz::RenderCamera) * cameraID.index, sizeof(hz::RenderCamera));
    commands.copyBuffer(readback, sizeof(hz::RenderCamera), frame.getBuffers().lights, sizeof(hz::RenderLight) * lightID.index,
                        sizeof(hz::RenderLight));
    const hz::SubmitHandle submission = context.submit(commands);
    pool.retire(std::move(frame), submission);
    frame = pool.capture();
    EXPECT_FLOAT_EQ((float)frame.getCameras().pData[cameraID.index].current.getElem(3, 0), 2);
    EXPECT_FLOAT_EQ((float)frame.getCameras().pData[cameraID.index].previous.getElem(3, 0), 1);
    EXPECT_FLOAT_EQ(frame.getCameras().pData[cameraID.index].previousLens.verticalFov, 0.8f);
    EXPECT_FLOAT_EQ(frame.getLights().pData[lightID.index].intensity, 8);
    frame = {};
    transform.translation = Vector3(3, 0, 0);
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
    for (uint32_t i = 0; i < 3; ++i)
    {
        frame = pool.capture();
        EXPECT_FLOAT_EQ((float)frame.getCameras().pData[cameraID.index].current.getElem(3, 0), 3);
        EXPECT_FLOAT_EQ((float)frame.getCameras().pData[cameraID.index].previous.getElem(3, 0), 1);
        EXPECT_FLOAT_EQ(frame.getCameras().pData[cameraID.index].previousLens.verticalFov, 0.8f);
        frame = {};
    }
    context.wait(submission);
    const hz::RenderCamera* copiedCamera = (const hz::RenderCamera*)readback.get()->pCpuMappedAddress;
    const hz::RenderLight*  copiedLight =
        (const hz::RenderLight*)((const uint8_t*)readback.get()->pCpuMappedAddress + sizeof(hz::RenderCamera));
    EXPECT_EQ(copiedCamera->id, cameraID);
    EXPECT_FLOAT_EQ(copiedCamera->lens.verticalFov, 0.8f);
    EXPECT_FLOAT_EQ((float)copiedCamera->current.getElem(3, 0), 1);
    EXPECT_EQ(copiedLight->id, lightID);
    EXPECT_FLOAT_EQ(copiedLight->intensity, 4);
    frame = pool.capture();
    hz::CommandList&       moving = context.acquireCommandList();
    const hz::SubmitHandle movingSubmission = context.submit(moving);
    pool.retire(std::move(frame), movingSubmission);
    context.wait(movingSubmission);
    for (uint32_t i = 0; i < 3; ++i)
    {
        frame = pool.capture();
        EXPECT_FLOAT_EQ((float)frame.getCameras().pData[cameraID.index].previous.getElem(3, 0), 3);
        EXPECT_FLOAT_EQ(frame.getCameras().pData[cameraID.index].previousLens.verticalFov, 0.6f);
        frame = {};
    }
    const MemoryTrackingStats before = memGetTrackingStats();
    for (uint32_t i = 0; i < 30; ++i)
    {
        transform.translation = Vector3((float)i + 4, 0, 0);
        lens.verticalFov = 0.6f + 0.001f * i;
        ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
        ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::camera, &lens));
        frame = pool.capture();
        EXPECT_FLOAT_EQ((float)frame.getCameras().pData[cameraID.index].current.getElem(3, 0), (float)i + 4);
        EXPECT_FLOAT_EQ((float)frame.getLights().pData[lightID.index].transform.getElem(3, 0), (float)i + 4);
        EXPECT_FLOAT_EQ((float)frame.getCameras().pData[cameraID.index].previous.getElem(3, 0), 3);
        frame = {};
    }
    const MemoryTrackingStats after = memGetTrackingStats();
    if (before.trackingEnabled)
    {
        EXPECT_EQ(before.totalAllocationCount, after.totalAllocationCount);
        EXPECT_EQ(before.reallocationCount, after.reallocationCount);
    }
    frame = pool.capture();
    transform.translation = Vector3(100, 0, 0);
    lens.verticalFov = 1;
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::localTransform, &transform));
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::camera, &lens));
    scene.resetHistory(entity);
    hz::CommandList&       cut = context.acquireCommandList();
    const hz::SubmitHandle cutSubmission = context.submit(cut);
    pool.retire(std::move(frame), cutSubmission);
    context.wait(cutSubmission);
    frame = pool.capture();
    EXPECT_FLOAT_EQ((float)frame.getCameras().pData[cameraID.index].previous.getElem(3, 0), 100);
    EXPECT_FLOAT_EQ(frame.getCameras().pData[cameraID.index].previousLens.verticalFov, 1);
    frame = {};
    // Removing only the mesh disables its emitter, keeping the light slot and texture lease.
    ASSERT_TRUE(world.removeComponent(entity, hz::SceneTypes::meshRenderer));
    scene.update();
    registry.update();
    EXPECT_EQ(scene.findLight(entity), lightID);
    EXPECT_EQ(scene.getLight(lightID)->ready, 0u);
    EXPECT_FALSE(scene.getLight(lightID)->object.isValid());
    EXPECT_EQ(registry.getState(textureHandle), hz::AssetState::Ready);
    light.type = hz::LightType::Dome;
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::light, &light));
    frame = pool.capture();
    const hz::GpuTextureID textureID = frame.getLights().pData[lightID.index].texture;
    ASSERT_TRUE(world.removeComponent(entity, hz::SceneTypes::light));
    ASSERT_TRUE(world.removeComponent(entity, hz::SceneTypes::camera));
    scene.update();
    registry.update();
    EXPECT_NE(frame.getAssets().getTexture(textureHandle), nullptr);
    EXPECT_NE(frame.getAssets().getGpuTables().getTexture(textureID), nullptr);
    hz::CommandList& retired = context.acquireCommandList();
    retired.copyBuffer(readback, sizeof(hz::RenderCamera), frame.getBuffers().lights, sizeof(hz::RenderLight) * lightID.index,
                       sizeof(hz::RenderLight));
    const hz::SubmitHandle retiredSubmission = context.submit(retired);
    pool.retire(std::move(frame), retiredSubmission);
    context.wait(retiredSubmission);
    EXPECT_EQ(copiedLight->texture, textureID);
    pool.collect();
    registry.update();
    EXPECT_EQ(registry.getCount(), 0u);
    for (uint32_t i = 0; i < 3; ++i)
    {
        frame = pool.capture();
        EXPECT_EQ(frame.getActiveLights().count, 0u);
        EXPECT_EQ(frame.getActiveCameras().count, 0u);
        EXPECT_FALSE(frame.getLights().pData[lightID.index].id.isValid());
        EXPECT_FALSE(frame.getCameras().pData[cameraID.index].id.isValid());
        EXPECT_EQ(frame.getAssets().getGpuTables().getTexture(textureID), nullptr);
        frame = {};
    }
    light.texture = {};
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::light, &light));
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::camera, &lens));
    frame = pool.capture();
    const hz::RenderCameraID reusedCamera = scene.findCamera(entity);
    EXPECT_EQ(reusedCamera.index, cameraID.index);
    EXPECT_NE(reusedCamera.generation, cameraID.generation);
    EXPECT_EQ(scene.findLight(entity).index, lightID.index);
    EXPECT_NE(scene.findLight(entity).generation, lightID.generation);
    EXPECT_FLOAT_EQ((float)frame.getCameras().pData[reusedCamera.index].previous.getElem(3, 0), 100);
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
    hz::AssetRegistry registry(catalog);
    loaders.registerLoaders(registry);
    const hz::AssetHandle source = registry.request(assets.getID());
    wait(registry, source);
    ASSERT_EQ(registry.getState(source), hz::AssetState::Ready);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::unique_ptr<hz::World> world = hz::make_unique<hz::World>(types);
    const hz::Entity          root = registry.getScene(source)->instantiate(*world);
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
    const hz::ObjectID rootID = world->getObjectID(root), firstID = world->getObjectID(first), secondID = world->getObjectID(second);
    hz::WorldDocument  saved;
    ASSERT_TRUE(saved.capture(*world));
    ASSERT_TRUE(saved.save(RD_OTHER_FILES, "edited.world.json"));
    hz::Array<char> original;
    ASSERT_TRUE(saved.serialize(original));
    hz::unique_ptr<hz::RenderScene> scene = hz::make_unique<hz::RenderScene>(*world, registry);
    scene->update();
    ASSERT_EQ(scene->getActiveDraws().count, 2u);
    ASSERT_TRUE(registry.release(source));
    hz::unique_ptr<hz::SceneFramePool> pool = hz::make_unique<hz::SceneFramePool>(*scene, context);
    hz::GPUBuffer         readback = context.createBuffer({ .size = sizeof(hz::RenderInstance) + sizeof(hz::GpuMaterialRecord) + 36,
                                                            .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                            .startState = RESOURCE_STATE_COPY_DEST,
                                                            .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    const hz::AssetHandle oldMesh = registry.find(renderer.mesh);
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
        hz::SceneFrameSnapshot frame = pool->capture();
        ASSERT_TRUE(frame.isValid());
        ASSERT_EQ(frame.getActiveDraws().count, 2u);
        const hz::RenderObjectID objectID = scene->findObject(entity);
        const hz::RenderObject&  object = frame.getObjects().pData[objectID.index];
        const uint32_t           instance = frame.getInstanceIndices().pData[object.instanceOffset];
        const hz::RenderDraw*    draw = nullptr;
        for (uint32_t index : frame.getActiveDraws())
            if (frame.getDraws().pData[index].object == objectID)
                draw = &frame.getDraws().pData[index];
        ASSERT_NE(draw, nullptr);
        EXPECT_EQ(draw->submesh, renderer.overrides[0].submesh);
        const hz::GpuMaterialRecord* material = frame.getAssets().getGpuTables().getMaterial(draw->material);
        ASSERT_NE(material, nullptr);
        EXPECT_NE(frame.getAssets().getGpuTables().getTexture(material->baseColorTexture), nullptr);
        const hz::MeshAsset* mesh = frame.getAssets().getMesh(registry.find(renderer.mesh));
        ASSERT_NE(mesh, nullptr);
        hz::CommandList& commands = context.acquireCommandList();
        commands.copyBuffer(readback, 0, frame.getBuffers().instances, instance * sizeof(hz::RenderInstance), sizeof(hz::RenderInstance));
        commands.copyBuffer(readback, sizeof(hz::RenderInstance), frame.getBuffers().materials,
                            draw->material.index * sizeof(hz::GpuMaterialRecord), sizeof(hz::GpuMaterialRecord));
        commands.copyBuffer(readback, sizeof(hz::RenderInstance) + sizeof(hz::GpuMaterialRecord), mesh->getVertexBuffer(0), 0, 36);
        const hz::SubmitHandle submission = context.submit(commands);
        pool->retire(std::move(frame), submission);
        // Destroying the pool drains its submitted frame before releasing World bindings and assets.
        pool = nullptr;
        EXPECT_TRUE(context.isComplete(submission));
        scene = nullptr;
        world = nullptr;
        registry.update();
        EXPECT_EQ(registry.getCount(), 0u);
        EXPECT_EQ(registry.getMesh(oldMesh), nullptr);
        const hz::RenderInstance* copied = (const hz::RenderInstance*)readback.get()->pCpuMappedAddress;
        EXPECT_EQ(copied->objectID, firstID);
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
        hz::WorldDocument loaded;
        ASSERT_TRUE(loaded.load(RD_OTHER_FILES, "edited.world.json"));
        ASSERT_TRUE(loaded.instantiate(types, world));
        EXPECT_EQ(loaded.getID(), saved.getID());
        ASSERT_TRUE(loaded.capture(*world));
        hz::Array<char> restored;
        ASSERT_TRUE(loaded.serialize(restored));
        ASSERT_EQ(restored.size(), original.size());
        EXPECT_EQ(memcmp(restored.data(), original.data(), original.size()), 0);
        scene = hz::make_unique<hz::RenderScene>(*world, registry);
        scene->update();
        const hz::AssetHandle reloadedMesh = registry.find(renderer.mesh);
        ASSERT_TRUE(reloadedMesh.isValid());
        EXPECT_NE(reloadedMesh, oldMesh);
        wait(registry, reloadedMesh);
        wait(registry, registry.find(assets.getMeshes().pData[1].id));
        scene->update();
        pool = hz::make_unique<hz::SceneFramePool>(*scene, context);
    }
    registry.shutdown();
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
        hz::AssetRegistry registry(catalog);
        loaders.registerLoaders(registry);
        const hz::AssetHandle mesh = registry.request(scene.getMeshes().pData[0].id);
        wait(registry, mesh);
        EXPECT_EQ(registry.getError(mesh), hz::AssetError::UploadFailed) << prefix;
        EXPECT_EQ(registry.getMesh(mesh), nullptr);
    }
    writeGeometry((IndexType)2);
    {
        hz::AssetRegistry registry(catalog);
        loaders.registerLoaders(registry);
        const hz::AssetHandle mesh = registry.request(scene.getMeshes().pData[0].id);
        wait(registry, mesh);
        EXPECT_EQ(registry.getError(mesh), hz::AssetError::UploadFailed);
        EXPECT_EQ(registry.getMesh(mesh), nullptr);
    }
    std::filesystem::remove(directory / "scene.bin");
    {
        hz::AssetRegistry registry(catalog);
        loaders.registerLoaders(registry);
        const hz::AssetHandle mesh = registry.request(scene.getMeshes().pData[0].id);
        wait(registry, mesh);
        EXPECT_EQ(registry.getError(mesh), hz::AssetError::UploadFailed);
    }
    writeGeometry();
    hz::AssetCatalogEntry wrong = *catalog.find(scene.getMeshes().pData[0].id);
    wrong.location.elementIndex = 1;
    ASSERT_TRUE(catalog.replace(wrong));
    {
        hz::AssetRegistry registry(catalog);
        loaders.registerLoaders(registry);
        const hz::AssetHandle mismatched = registry.request(wrong.id);
        const hz::AssetHandle root = registry.request(scene.getID());
        wait(registry, root);
        EXPECT_EQ(registry.getError(root), hz::AssetError::DependencyFailed);
        EXPECT_EQ(registry.getError(mismatched), hz::AssetError::LoadFailed);
    }
    wrong.location.elementIndex = 0;
    ASSERT_TRUE(catalog.replace(wrong));
    for (uint32_t phase = 0; phase < 2; ++phase)
    {
        hz::AssetRegistry registry(catalog);
        loaders.registerLoaders(registry);
        const hz::AssetHandle root = registry.request(scene.getID());
        if (phase)
        {
            for (uint32_t i = 0; i < 5000; ++i)
            {
                registry.update();
                if (registry.getState(registry.find(wrong.id)) == hz::AssetState::UploadingGPU)
                    break;
                threadSleep(1);
            }
            EXPECT_EQ(registry.getState(registry.find(wrong.id)), hz::AssetState::UploadingGPU);
        }
        else
            registry.update();
        registry.release(root);
        registry.shutdown();
        EXPECT_EQ(registry.getCount(), 0u);
    }
}
