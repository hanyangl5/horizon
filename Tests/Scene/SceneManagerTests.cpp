#include <gtest/gtest.h>

#include <string.h>

#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"
#include "Scene/ISceneManager.h"
#include "Scene/SceneGeometry.h"

namespace
{
const char* kValidManifest = R"json({
  "version": 1,
  "contentHash": "fnv1a64:0123456789abcdef",
  "dependencies": ["courtyard.gltf", "Buffers/courtyard.bin", "Textures/albedo.dds"],
  "defaultScene": "courtyard",
  "scenes": {
    "courtyard": {
      "geometry": "Meshes/courtyard.bin",
      "sourceGltf": "courtyard.gltf",
      "textureDirectories": ["objects", "textures"],
      "textures": [
        { "path": "Textures/albedo.dds", "srgb": true },
        { "path": "Textures/normal.dds", "srgb": false }
      ],
      "materials": [
        {
          "name": "stone",
          "baseColorTexture": 0,
          "normalTexture": 1,
          "metallicRoughnessTexture": -1,
          "emissiveTexture": -1,
          "baseColorFactor": [1.0, 0.5, 0.25, 1.0],
          "metallicFactor": 0.2,
          "roughnessFactor": 0.8,
          "emissiveFactor": [0.0, 0.1, 0.2]
        }
      ],
      "environment": "Textures/environment.dds",
      "materialConvention": {
        "baseColor": "RGB base color, A opacity",
        "specular": "R AO, G roughness, B metalness",
        "normal": "DirectX normal map",
        "emissive": "RGB emissive"
      }
    }
  }
})json";

struct FakeSceneAssetBackend
{
    bool     mCompleted;
    bool     mFailTexture;
    uint32_t mGeometryLoadCount;
    uint32_t mTextureLoadCount;
    uint32_t mBufferLoadCount;
    uint32_t mRemoveCount;
    Geometry* pGeometry;
};

// Keep completion under test control, but use real resources so adoption and destruction are exercised.
Buffer* loadTestBuffer(DescriptorType descriptors, ResourceState state)
{
    Buffer* pBuffer = nullptr;
    BufferLoadDesc desc = {
        .ppBuffer = &pBuffer,
        .mDesc = {
            .mSize = 256,
            .mElementCount = 64,
            .mStructStride = 4,
            .mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
            .mStartState = state,
            .mDescriptors = descriptors,
        },
    };
    SyncToken token = 0;
    addResource(&desc, &token);
    waitForToken(&token);
    return pBuffer;
}

Geometry* loadTestGeometry()
{
    Geometry* pGeometry = (Geometry*)tf_calloc(1, sizeof(Geometry) + sizeof(IndirectDrawIndexArguments));
    pGeometry->pIndexBuffer = loadTestBuffer(DESCRIPTOR_TYPE_INDEX_BUFFER, gIndexBufferState);
    pGeometry->pVertexBuffers[0] = loadTestBuffer(DESCRIPTOR_TYPE_VERTEX_BUFFER, gVertexBufferState);
    pGeometry->mVertexBufferCount = 1;
    pGeometry->mVertexStrides[0] = 12;
    pGeometry->mIndexCount = 3;
    pGeometry->mVertexCount = 3;
    pGeometry->mDrawArgCount = 1;
    pGeometry->pDrawArgs = (IndirectDrawIndexArguments*)(pGeometry + 1);
    pGeometry->pDrawArgs[0].mIndexCount = 3;
    return pGeometry;
}

Texture* loadTestTexture()
{
    Texture* pTexture = nullptr;
    TextureDesc texture = {
        .mWidth = 1, .mHeight = 1, .mDepth = 1, .mArraySize = 1, .mMipLevels = 1,
        .mSampleCount = SAMPLE_COUNT_1, .mFormat = TinyImageFormat_R8G8B8A8_UNORM,
        .mStartState = RESOURCE_STATE_SHADER_RESOURCE, .mDescriptors = DESCRIPTOR_TYPE_TEXTURE,
    };
    TextureLoadDesc desc = { .ppTexture = &pTexture, .pDesc = &texture };
    SyncToken token = 0;
    addResource(&desc, &token);
    waitForToken(&token);
    return pTexture;
}

void fakeLoadSceneGeometry(GeometryLoadDesc* pDesc, SyncToken* pToken, void* pUserData)
{
    FakeSceneAssetBackend* pBackend = (FakeSceneAssetBackend*)pUserData;
    ++pBackend->mGeometryLoadCount;
    *pDesc->ppGeometry = pBackend->pGeometry = loadTestGeometry();
    *pToken = 7;
}

void fakeLoadSceneTexture(TextureLoadDesc* pDesc, SyncToken* pToken, void* pUserData)
{
    FakeSceneAssetBackend* pBackend = (FakeSceneAssetBackend*)pUserData;
    ++pBackend->mTextureLoadCount;
    if (!pBackend->mFailTexture)
        *pDesc->ppTexture = loadTestTexture();
    *pToken = 7;
}

void fakeLoadSceneBuffer(BufferLoadDesc* pDesc, SyncToken* pToken, void* pUserData)
{
    FakeSceneAssetBackend* pBackend = (FakeSceneAssetBackend*)pUserData;
    ++pBackend->mBufferLoadCount;
    SyncToken token = 0;
    addResource(pDesc, &token);
    waitForToken(&token);
    *pToken = 7;
}

bool fakeIsSceneTokenCompleted(const SyncToken*, void* pUserData) { return ((FakeSceneAssetBackend*)pUserData)->mCompleted; }

void fakeWaitForSceneToken(const SyncToken*, void* pUserData) { ((FakeSceneAssetBackend*)pUserData)->mCompleted = true; }

void fakeRemoveSceneGeometry(void* pUserData, void* pResource)
{
    ++((FakeSceneAssetBackend*)pUserData)->mRemoveCount;
    removeResource((Geometry*)pResource);
}
void fakeRemoveSceneTexture(void* pUserData, void* pResource)
{
    ++((FakeSceneAssetBackend*)pUserData)->mRemoveCount;
    removeResource((Texture*)pResource);
}
void fakeRemoveSceneBuffer(void* pUserData, void* pResource)
{
    ++((FakeSceneAssetBackend*)pUserData)->mRemoveCount;
    removeResource((Buffer*)pResource);
}
} // namespace

TEST(SceneAssetManifestTest, ParsesSelectedSceneAndMaterialMetadata)
{
    SceneAssetManifest manifest = {};
    SceneAssetError    error = {};

    ASSERT_TRUE(parseSceneAssetManifest(kValidManifest, strlen(kValidManifest), &manifest, &error));
    EXPECT_EQ(error.mCode, SCENE_ASSET_ERROR_NONE);
    EXPECT_EQ(manifest.mVersion, SCENE_ASSET_MANIFEST_VERSION);
    EXPECT_STREQ(manifest.mContentHash, "fnv1a64:0123456789abcdef");
    ASSERT_EQ(manifest.mDependencyCount, 3u);
    EXPECT_STREQ(manifest.mDependencies[0], "courtyard.gltf");
    EXPECT_STREQ(manifest.mDependencies[1], "Buffers/courtyard.bin");
    EXPECT_STREQ(manifest.mDependencies[2], "Textures/albedo.dds");
    EXPECT_STREQ(manifest.mDefaultScene, "courtyard");
    EXPECT_STREQ(manifest.mGeometry, "Meshes/courtyard.bin");
    EXPECT_STREQ(manifest.mSourceGltf, "courtyard.gltf");
    EXPECT_STREQ(manifest.mEnvironment, "Textures/environment.dds");
    ASSERT_EQ(manifest.mTextureDirectoryCount, 2u);
    EXPECT_STREQ(manifest.mTextureDirectories[0], "objects");
    EXPECT_STREQ(manifest.mTextureDirectories[1], "textures");
    ASSERT_EQ(manifest.mTextureCount, 2u);
    EXPECT_STREQ(manifest.mTextures[0].mPath, "Textures/albedo.dds");
    EXPECT_TRUE(manifest.mTextures[0].mSrgb);
    EXPECT_STREQ(manifest.mTextures[1].mPath, "Textures/normal.dds");
    EXPECT_FALSE(manifest.mTextures[1].mSrgb);
    ASSERT_EQ(manifest.mMaterialCount, 1u);
    EXPECT_STREQ(manifest.mMaterials[0].mName, "stone");
    EXPECT_EQ(manifest.mMaterials[0].mBaseColorTexture, 0);
    EXPECT_EQ(manifest.mMaterials[0].mNormalTexture, 1);
    EXPECT_EQ(manifest.mMaterials[0].mMetallicRoughnessTexture, -1);
    EXPECT_FLOAT_EQ(manifest.mMaterials[0].mBaseColorFactor[1], 0.5f);
    EXPECT_FLOAT_EQ(manifest.mMaterials[0].mMetallicFactor, 0.2f);
    EXPECT_FLOAT_EQ(manifest.mMaterials[0].mRoughnessFactor, 0.8f);
    EXPECT_FLOAT_EQ(manifest.mMaterials[0].mEmissiveFactor[2], 0.2f);
    EXPECT_STREQ(manifest.mMaterialConvention.mBaseColor, "RGB base color, A opacity");
    EXPECT_STREQ(manifest.mMaterialConvention.mSpecular, "R AO, G roughness, B metalness");
    EXPECT_STREQ(manifest.mMaterialConvention.mNormal, "DirectX normal map");
    EXPECT_STREQ(manifest.mMaterialConvention.mEmissive, "RGB emissive");
}

TEST(SceneAssetManifestTest, RejectsMalformedOrUnsupportedManifests)
{
    struct InvalidManifestCase
    {
        const char*         pJson;
        SceneAssetErrorCode mExpectedCode;
    };

    const InvalidManifestCase cases[] = {
        { "{", SCENE_ASSET_ERROR_INVALID_JSON },
        { R"({"version":2,"defaultScene":"main","scenes":{"main":{"geometry":"mesh.bin"}}})", SCENE_ASSET_ERROR_UNSUPPORTED_VERSION },
        { R"({"version":1,"defaultScene":"missing","scenes":{"main":{"geometry":"mesh.bin"}}})", SCENE_ASSET_ERROR_MISSING_FIELD },
        { R"({"version":1,"defaultScene":"main","scenes":{"main":{"geometry":"C:/mesh.bin"}}})", SCENE_ASSET_ERROR_INVALID_PATH },
        { R"({"version":1,"defaultScene":"main","scenes":{"main":{"geometry":"../mesh.bin"}}})", SCENE_ASSET_ERROR_INVALID_PATH },
        { R"({"version":1,"contentHash":"sha256:not-supported","defaultScene":"main","scenes":{"main":{"geometry":"mesh.bin"}}})",
          SCENE_ASSET_ERROR_INVALID_FIELD },
        { R"({"version":1,"dependencies":["../mesh.bin"],"defaultScene":"main","scenes":{"main":{"geometry":"mesh.bin"}}})",
          SCENE_ASSET_ERROR_INVALID_PATH },
        { R"({"version":1,"defaultScene":"main","scenes":{"main":{"geometry":"mesh.bin","textures":[{"path":"raw.png"}]}}})",
          SCENE_ASSET_ERROR_INVALID_FIELD },
    };

    for (const InvalidManifestCase& testCase : cases)
    {
        SceneAssetManifest manifest = {};
        SceneAssetError    error = {};
        EXPECT_FALSE(parseSceneAssetManifest(testCase.pJson, strlen(testCase.pJson), &manifest, &error));
        EXPECT_EQ(error.mCode, testCase.mExpectedCode);
        EXPECT_NE(error.mMessage[0], '\0');
    }
}

TEST(SceneAssetManifestTest, PreparesExistingGeometryLoaderDescriptor)
{
    SceneAssetManifest manifest = {};
    SceneAssetError    error = {};
    ASSERT_TRUE(parseSceneAssetManifest(kValidManifest, strlen(kValidManifest), &manifest, &error));

    Geometry*        pGeometry = nullptr;
    GeometryData*    pGeometryData = nullptr;
    VertexLayout     vertexLayout = {};
    GeometryLoadDesc source = {
        .ppGeometry = &pGeometry,
        .ppGeometryData = &pGeometryData,
        .pFileName = "ignored.bin",
        .mFlags = GEOMETRY_LOAD_FLAG_SHADOWED,
        .pVertexLayout = &vertexLayout,
    };
    GeometryLoadDesc prepared = {};

    ASSERT_TRUE(prepareSceneAssetGeometryLoadDesc(&manifest, &source, &prepared, &error));
    EXPECT_STREQ(prepared.pFileName, "Meshes/courtyard.bin");
    EXPECT_EQ(prepared.ppGeometry, &pGeometry);
    EXPECT_EQ(prepared.ppGeometryData, &pGeometryData);
    EXPECT_EQ(prepared.mFlags, GEOMETRY_LOAD_FLAG_SHADOWED);
    EXPECT_EQ(prepared.pVertexLayout, &vertexLayout);
}

class SceneAssetFileTest: public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        ASSERT_TRUE(initMemAlloc(nullptr));
        FileSystemInitDesc desc = {
            .pAppName = "ResourcesTests",
        };
        ASSERT_TRUE(initFileSystem(&desc));
        fsSetPathForResourceDir(pSystemFileIO, RM_PROJECT, RD_OTHER_FILES, "Tests/Resources/Fixtures");
    }

    static void TearDownTestSuite()
    {
        exitFileSystem();
        exitMemAlloc();
    }
};

TEST_F(SceneAssetFileTest, LoadsManifestThroughResourceDirectory)
{
    SceneAssetManifest manifest = {};
    SceneAssetError    error = {};

    ASSERT_TRUE(loadSceneAssetManifest(RD_OTHER_FILES, "valid.scene.json", &manifest, &error));
    EXPECT_STREQ(manifest.mDefaultScene, "fixture");
    EXPECT_STREQ(manifest.mGeometry, "Meshes/fixture.bin");
    ASSERT_EQ(manifest.mTextureDirectoryCount, 1u);
    EXPECT_STREQ(manifest.mTextureDirectories[0], "Textures");
}

class SceneManagerTest: public SceneAssetFileTest
{
protected:
    hz::RenderContext context;
    void SetUp() override
    {
#if defined(_WINDOWS)
        const hz::ContextDesc desc = { .pAppName = "SceneManagerTest" };
        ASSERT_TRUE(context.init(desc));
#else
        GTEST_SKIP() << "Scene GPU ownership requires the D3D12 backend";
#endif
    }
};

TEST_F(SceneManagerTest, RequestsSceneDirectlyFromManifestFile)
{
    FakeSceneAssetBackend backend = {};
    SceneManagerDesc  systemDesc = {
         .mCapacity = 1, .pContext = &context,
         .mCallbacks = {
             .pLoadGeometry = fakeLoadSceneGeometry,
             .pLoadTexture = fakeLoadSceneTexture,
             .pLoadBuffer = fakeLoadSceneBuffer,
             .pIsTokenCompleted = fakeIsSceneTokenCompleted,
             .pWaitForToken = fakeWaitForSceneToken,
             .pRemoveGeometry = fakeRemoveSceneGeometry,
             .pRemoveTexture = fakeRemoveSceneTexture,
             .pRemoveBuffer = fakeRemoveSceneBuffer,
         },
         .pUserData = &backend,
    };
    SceneManager* pSystem = nullptr;
    ASSERT_TRUE(initSceneManager(&systemDesc, &pSystem));

    Geometry*        pGeometry = nullptr;
    VertexLayout     vertexLayout = {};
    GeometryLoadDesc geometryLoad = {
        .ppGeometry = &pGeometry,
        .pVertexLayout = &vertexLayout,
    };
    SceneAssetError  error = {};
    SceneAssetHandle handle = requestSceneAsset(pSystem, RD_OTHER_FILES, "valid.scene.json", &geometryLoad, &error);
    EXPECT_TRUE(isSceneAssetHandleValid(handle)) << error.mMessage;
    EXPECT_EQ(backend.mGeometryLoadCount, 1u);

    backend.mCompleted = true;
    updateSceneManager(pSystem);
    releaseSceneAsset(pSystem, handle);
    exitSceneManager(pSystem);
}

TEST_F(SceneManagerTest, AsyncHandleTransitionsAndRejectsStaleGenerations)
{
    SceneAssetManifest manifest = {};
    SceneAssetError    error = {};
    ASSERT_TRUE(parseSceneAssetManifest(kValidManifest, strlen(kValidManifest), &manifest, &error));

    FakeSceneAssetBackend backend = {};
    SceneManagerDesc  systemDesc = {
         .mCapacity = 1, .pContext = &context,
         .mCallbacks = {
             .pLoadGeometry = fakeLoadSceneGeometry,
             .pLoadTexture = fakeLoadSceneTexture,
             .pLoadBuffer = fakeLoadSceneBuffer,
             .pIsTokenCompleted = fakeIsSceneTokenCompleted,
             .pWaitForToken = fakeWaitForSceneToken,
             .pRemoveGeometry = fakeRemoveSceneGeometry,
             .pRemoveTexture = fakeRemoveSceneTexture,
             .pRemoveBuffer = fakeRemoveSceneBuffer,
         },
         .pUserData = &backend,
    };
    SceneManager* pSystem = nullptr;
    ASSERT_TRUE(initSceneManager(&systemDesc, &pSystem));

    VertexLayout     vertexLayout = {};
    Geometry*        pGeometry = nullptr;
    GeometryLoadDesc geometryLoad = {
        .ppGeometry = &pGeometry,
        .pVertexLayout = &vertexLayout,
    };
    GeometryBuffer sharedGeometry = {};
    geometryLoad.pGeometryBuffer = &sharedGeometry;
    EXPECT_FALSE(isSceneAssetHandleValid(requestSceneAssetFromManifest(pSystem, &manifest, &geometryLoad)));
    EXPECT_EQ(backend.mGeometryLoadCount, 0u);
    geometryLoad.pGeometryBuffer = nullptr;
    SceneAssetHandle first = requestSceneAssetFromManifest(pSystem, &manifest, &geometryLoad);
    ASSERT_TRUE(isSceneAssetHandleValid(first));
    EXPECT_EQ(getSceneAssetStatus(pSystem, first), SCENE_ASSET_STATUS_LOADING);
    EXPECT_EQ(backend.mGeometryLoadCount, 1u);
    EXPECT_EQ(backend.mTextureLoadCount, 2u);
    EXPECT_EQ(backend.mBufferLoadCount, 1u);
    EXPECT_EQ(getSceneAssetGeometry(pSystem, first), nullptr);

    backend.mCompleted = true;
    updateSceneManager(pSystem);
    EXPECT_EQ(getSceneAssetStatus(pSystem, first), SCENE_ASSET_STATUS_READY);
    const SceneGeometry* geometry = getSceneAssetGeometry(pSystem, first);
    ASSERT_NE(geometry, nullptr);
    EXPECT_TRUE(geometry->mIndexBuffer);
    EXPECT_TRUE(geometry->mVertexBuffers[0]);
    EXPECT_EQ(geometry->mVertexStrides[0], 12u);
    EXPECT_EQ(geometry->pDrawArgs[0].mIndexCount, 3u);
    EXPECT_EQ(backend.pGeometry->pIndexBuffer, nullptr);
    EXPECT_EQ(backend.pGeometry->pVertexBuffers[0], nullptr);
    ASSERT_EQ(getSceneAssetTextureCount(pSystem, first), 2u);
    ASSERT_NE(getSceneAssetTexture(pSystem, first, 1), nullptr);
    EXPECT_TRUE(*getSceneAssetTexture(pSystem, first, 1));
    EXPECT_EQ(getSceneAssetTexture(pSystem, first, 2), nullptr);
    ASSERT_NE(getSceneAssetMaterialBuffer(pSystem, first), nullptr);
    EXPECT_TRUE(*getSceneAssetMaterialBuffer(pSystem, first));
    ASSERT_EQ(getSceneAssetMaterialCount(pSystem, first), 1u);
    const SceneAssetGpuMaterial* pGpuMaterials = getSceneAssetGpuMaterials(pSystem, first);
    ASSERT_NE(pGpuMaterials, nullptr);
    EXPECT_EQ(pGpuMaterials[0].mBaseColorTexture, 0u);
    EXPECT_EQ(pGpuMaterials[0].mNormalTexture, 1u);
    EXPECT_EQ(pGpuMaterials[0].mMetallicRoughnessTexture, UINT32_MAX);
    EXPECT_FLOAT_EQ(pGpuMaterials[0].mBaseColorFactor[1], 0.5f);
    ASSERT_NE(getSceneAssetManifest(pSystem, first), nullptr);
    EXPECT_EQ(getSceneAssetManifest(pSystem, first)->mMaterialCount, 1u);

    ASSERT_TRUE(releaseSceneAsset(pSystem, first));
    EXPECT_EQ(getSceneAssetStatus(pSystem, first), SCENE_ASSET_STATUS_INVALID);
    EXPECT_EQ(backend.mRemoveCount, 1u); // Only CPU geometry remains on the loader cleanup path.
    EXPECT_EQ(getSceneAssetTexture(pSystem, first, 0), nullptr);
    EXPECT_EQ(getSceneAssetMaterialBuffer(pSystem, first), nullptr);

    backend.mCompleted = false;
    SceneAssetHandle second = requestSceneAssetFromManifest(pSystem, &manifest, &geometryLoad);
    ASSERT_TRUE(isSceneAssetHandleValid(second));
    EXPECT_EQ(second.mIndex, first.mIndex);
    EXPECT_NE(second.mGeneration, first.mGeneration);

    ASSERT_TRUE(releaseSceneAsset(pSystem, second));
    SceneAssetHandle whileRetiring = requestSceneAssetFromManifest(pSystem, &manifest, &geometryLoad);
    EXPECT_FALSE(isSceneAssetHandleValid(whileRetiring));

    backend.mCompleted = true;
    updateSceneManager(pSystem);
    EXPECT_EQ(backend.mRemoveCount, 5u); // Cancelled uploads never transfer ownership.
    SceneAssetHandle afterRetirement = requestSceneAssetFromManifest(pSystem, &manifest, &geometryLoad);
    EXPECT_TRUE(isSceneAssetHandleValid(afterRetirement));

    backend.mCompleted = true;
    updateSceneManager(pSystem);
    releaseSceneAsset(pSystem, afterRetirement);
    exitSceneManager(pSystem);
}

namespace
{
struct ProgressiveBackend
{
    SyncToken mNextToken;
    SyncToken mCompletedThrough;
};
void progressiveGeometry(GeometryLoadDesc* d, SyncToken* t, void* u)
{
    auto* b = (ProgressiveBackend*)u;
    *t = ++b->mNextToken;
    *d->ppGeometry = loadTestGeometry();
}
void progressiveTexture(TextureLoadDesc* d, SyncToken* t, void* u)
{
    auto* b = (ProgressiveBackend*)u;
    *t = ++b->mNextToken;
    *d->ppTexture = loadTestTexture();
}
void progressiveBuffer(BufferLoadDesc* d, SyncToken* t, void* u)
{
    auto* b = (ProgressiveBackend*)u;
    *t = ++b->mNextToken;
    SyncToken token = 0;
    addResource(d, &token);
    waitForToken(&token);
}
bool progressiveDone(const SyncToken* t, void* u) { return *t <= ((ProgressiveBackend*)u)->mCompletedThrough; }
void progressiveWait(const SyncToken* t, void* u) { ((ProgressiveBackend*)u)->mCompletedThrough = *t; }
void progressiveRemoveGeometry(void*, void* p) { removeResource((Geometry*)p); }
void progressiveRemoveTexture(void*, void* p) { removeResource((Texture*)p); }
void progressiveRemoveBuffer(void*, void* p) { removeResource((Buffer*)p); }
} // namespace

TEST_F(SceneManagerTest, PublishesGeometryTexturesAndMaterialsIndependently)
{
    SceneAssetManifest manifest = {};
    SceneAssetError    error = {};
    ASSERT_TRUE(parseSceneAssetManifest(kValidManifest, strlen(kValidManifest), &manifest, &error));
    ProgressiveBackend   backend = {};
    SceneManagerDesc sd = { .mCapacity = 1, .pContext = &context,
                                .mCallbacks = { progressiveGeometry, progressiveTexture, progressiveBuffer, progressiveDone,
                                                progressiveWait, progressiveRemoveGeometry, progressiveRemoveTexture, progressiveRemoveBuffer },
                                .pUserData = &backend };
    SceneManager*    system = nullptr;
    ASSERT_TRUE(initSceneManager(&sd, &system));
    VertexLayout     layout = {};
    Geometry*        geometry = nullptr;
    GeometryLoadDesc gd = { .ppGeometry = &geometry, .pVertexLayout = &layout };
    SceneAssetHandle h = requestSceneAssetFromManifest(system, &manifest, &gd);
    ASSERT_TRUE(isSceneAssetHandleValid(h));
    EXPECT_FALSE(isSceneAssetGeometryResident(system, h));
    EXPECT_FALSE(isSceneAssetTextureResident(system, h, 0));
    backend.mCompletedThrough = 1;
    updateSceneManager(system);
    EXPECT_TRUE(isSceneAssetGeometryResident(system, h));
    ASSERT_NE(getSceneAssetGeometry(system, h), nullptr);
    EXPECT_TRUE(getSceneAssetGeometry(system, h)->mIndexBuffer);
    EXPECT_FALSE(isSceneAssetTextureResident(system, h, 0));
    EXPECT_EQ(getSceneAssetStatus(system, h), SCENE_ASSET_STATUS_LOADING);
    backend.mCompletedThrough = 2;
    updateSceneManager(system);
    EXPECT_TRUE(isSceneAssetTextureResident(system, h, 0));
    EXPECT_FALSE(isSceneAssetTextureResident(system, h, 1));
    EXPECT_NE(getSceneAssetTexture(system, h, 0), nullptr);
    EXPECT_EQ(getSceneAssetTexture(system, h, 1), nullptr);
    backend.mCompletedThrough = 4;
    updateSceneManager(system);
    EXPECT_TRUE(isSceneAssetMaterialResident(system, h));
    EXPECT_EQ(getSceneAssetStatus(system, h), SCENE_ASSET_STATUS_READY);
    releaseSceneAsset(system, h);

    // Retiring a partially adopted scene must free owners and pending native outputs once each.
    backend = {};
    h = requestSceneAssetFromManifest(system, &manifest, &gd);
    backend.mCompletedThrough = 1;
    updateSceneManager(system);
    ASSERT_TRUE(isSceneAssetGeometryResident(system, h));
    ASSERT_TRUE(releaseSceneAsset(system, h));
    EXPECT_EQ(getSceneAssetGeometry(system, h), nullptr);
    EXPECT_FALSE(isSceneAssetHandleValid(requestSceneAssetFromManifest(system, &manifest, &gd)));
    backend.mCompletedThrough = 4;
    updateSceneManager(system);
    const SceneAssetHandle next = requestSceneAssetFromManifest(system, &manifest, &gd);
    EXPECT_TRUE(isSceneAssetHandleValid(next));
    EXPECT_NE(next.mGeneration, h.mGeneration);
    exitSceneManager(system);
}

TEST_F(SceneManagerTest, GltfCookFailureDoesNotEnqueueGpuLoadsOrConsumeSlot)
{
    FakeSceneAssetBackend backend = {};
    static uint32_t cookCalls = 0;
    cookCalls = 0;
    const SceneManagerDesc desc = {
        .mCapacity = 1, .pContext = &context,
        .mCallbacks = {
            .pLoadGeometry = fakeLoadSceneGeometry,
            .pLoadTexture = fakeLoadSceneTexture,
            .pLoadBuffer = fakeLoadSceneBuffer,
            .pIsTokenCompleted = fakeIsSceneTokenCompleted,
            .pWaitForToken = fakeWaitForSceneToken,
            .pRemoveGeometry = fakeRemoveSceneGeometry,
            .pRemoveTexture = fakeRemoveSceneTexture,
            .pRemoveBuffer = fakeRemoveSceneBuffer,
        },
        .pUserData = &backend,
        .pEnsureGltfCooked = [](ResourceDirectory source, const char* file, ResourceDirectory output, SceneAssetError* error)
        {
            ++cookCalls;
            EXPECT_EQ(source, RD_TEXTURES);
            EXPECT_EQ(output, RD_OTHER_FILES);
            EXPECT_STREQ(file, "courtyard.gltf");
            error->mCode = SCENE_ASSET_ERROR_IO;
            strcpy(error->mMessage, "Cook failed");
            return false;
        },
    };
    SceneManager* manager = nullptr;
    ASSERT_TRUE(initSceneManager(&desc, &manager));
    VertexLayout layout = {};
    const GeometryLoadDesc geometry = { .pVertexLayout = &layout };
    SceneAssetError error = {};
    EXPECT_FALSE(isSceneAssetHandleValid(requestSceneAssetFromGltf(manager, RD_TEXTURES, "../bad.gltf", RD_OTHER_FILES, &geometry, &error)));
    EXPECT_EQ(cookCalls, 0u);
    EXPECT_FALSE(isSceneAssetHandleValid(requestSceneAssetFromGltf(manager, RD_TEXTURES, "courtyard.gltf", RD_OTHER_FILES, &geometry, &error)));
    EXPECT_EQ(cookCalls, 1u);
    EXPECT_STREQ(error.mMessage, "Cook failed");
    EXPECT_EQ(backend.mGeometryLoadCount, 0u);
    EXPECT_EQ(backend.mTextureLoadCount, 0u);
    EXPECT_EQ(backend.mBufferLoadCount, 0u);
    SceneAssetManifest manifest = {};
    EXPECT_TRUE(parseSceneAssetManifest(kValidManifest, strlen(kValidManifest), &manifest, &error));
    EXPECT_TRUE(isSceneAssetHandleValid(requestSceneAssetFromManifest(manager, &manifest, &geometry)));
    exitSceneManager(manager);
}

TEST_F(SceneManagerTest, ReportsFailedWhenAnAsyncResourceDoesNotLoad)
{
    FakeSceneAssetBackend backend = {
        .mCompleted = false,
        .mFailTexture = true,
    };
    SceneManagerDesc systemDesc = {
        .mCapacity = 1, .pContext = &context,
        .mCallbacks = {
            .pLoadGeometry = fakeLoadSceneGeometry,
            .pLoadTexture = fakeLoadSceneTexture,
            .pLoadBuffer = fakeLoadSceneBuffer,
            .pIsTokenCompleted = fakeIsSceneTokenCompleted,
            .pWaitForToken = fakeWaitForSceneToken,
            .pRemoveGeometry = fakeRemoveSceneGeometry,
            .pRemoveTexture = fakeRemoveSceneTexture,
            .pRemoveBuffer = fakeRemoveSceneBuffer,
        },
        .pUserData = &backend,
    };
    SceneManager* pSystem = nullptr;
    ASSERT_TRUE(initSceneManager(&systemDesc, &pSystem));

    SceneAssetManifest manifest = {};
    SceneAssetError    error = {};
    ASSERT_TRUE(parseSceneAssetManifest(kValidManifest, strlen(kValidManifest), &manifest, &error));
    Geometry*        pGeometry = nullptr;
    VertexLayout     vertexLayout = {};
    GeometryLoadDesc geometryLoad = {
        .ppGeometry = &pGeometry,
        .pVertexLayout = &vertexLayout,
    };
    const SceneAssetHandle handle = requestSceneAssetFromManifest(pSystem, &manifest, &geometryLoad);
    ASSERT_TRUE(isSceneAssetHandleValid(handle));

    backend.mCompleted = true;
    updateSceneManager(pSystem);
    EXPECT_EQ(getSceneAssetStatus(pSystem, handle), SCENE_ASSET_STATUS_FAILED);
    ASSERT_NE(getSceneAssetGeometry(pSystem, handle), nullptr);
    EXPECT_TRUE(getSceneAssetGeometry(pSystem, handle)->mIndexBuffer);
    EXPECT_EQ(getSceneAssetTexture(pSystem, handle, 0), nullptr);
    EXPECT_EQ(getSceneAssetTexture(pSystem, handle, 1), nullptr);
    EXPECT_TRUE(releaseSceneAsset(pSystem, handle));
    exitSceneManager(pSystem);
}
