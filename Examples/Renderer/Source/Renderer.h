#pragma once
#include "Application/IApp.h"
#include "Application/IFreeCameraController.h"
#include "Core/ILog.h"
#include "Core/IMath.h"
#include "Core/IToolFileSystem.h"
#include "Core/IUniquePtr.h"
#include "Profiler/IProfiler.h"
#include "Graphics/RenderContext.h"
#include "Core/IThread.h"
#include "Scene/AssetLoaders.h"
#include "Scene/SceneAsset.h"
#if defined(HORIZON_RENDERER_ASSET_COOKING)
#include "AssetPipeline/IAssetPipeline.h"
#endif

#include "RenderPasses.h"

constexpr const char* kSceneDirectory = "Assets/Bistro";
constexpr const char* kSceneSource = "BistroExterior.gltf";
constexpr bool        kEnableHDR = true;
// GPU-based validation slows GPU work several times over, so only Debug builds enable it by default.
#if defined(_DEBUG)
constexpr bool kEnableGpuValidation = true;
#else
constexpr bool kEnableGpuValidation = false;
#endif

const VertexLayout kSceneVertexLayout = {
    .bindings = { { .stride = 12 }, { .stride = 4 }, { .stride = 4 } },
    .attribs = {
        { .semantic = SEMANTIC_POSITION, .format = hz::Format::R32G32B32_SFLOAT, .binding = 0, .location = 0 },
        { .semantic = SEMANTIC_NORMAL, .format = hz::Format::R32_UINT, .binding = 1, .location = 1 },
        { .semantic = SEMANTIC_TEXCOORD0, .format = hz::Format::R32_UINT, .binding = 2, .location = 2 },
    },
    .bindingCount = 3, .attribCount = 3,
};

class RendererApp final: public IApp
{
public:
    RendererApp()
    {
        settings.width = 3840;
        settings.height = 2160;
        settings.vSyncEnabled = true;
        settings.showPlatformUI = false;
    }

    bool Init() override
    {
        configureResourceDirectories();
        initRenderContext();
        if (!initSceneAsset())
        {
            Exit();
            return false;
        }
        initCamera();
        return initRenderPasses();
    }

private:
    void configureResourceDirectories()
    {
        fsSetPathForResourceDir(pSystemFileIO, RM_SAVE_0, RD_MESHES, "RendererAssets");
        fsSetPathForResourceDir(pSystemFileIO, RM_SAVE_0, RD_OTHER_FILES, "RendererAssets");
        fsSetPathForResourceDir(pSystemFileIO, RM_PROJECT, RD_TEXTURES, kSceneDirectory);
        fsSetPathForResourceDir(pSystemFileIO, RM_PROJECT, RD_SHADER_SOURCES, "Examples/Renderer/Shaders");
    }

    void initRenderContext()
    {
        const hz::ContextDesc desc = {
            .pAppName = GetName(),
            .windowHandle = pWindow->handle,
            .imageCount = 3,
            .enableHDR = kEnableHDR,
            .enableVSync = settings.vSyncEnabled,
            .enableGpuValidation = kEnableGpuValidation,
            .enableGpuProfiler = true,
        };
        pContext = hz::make_unique<hz::RenderContext>(desc);
    }

    bool initSceneAsset()
    {
#if defined(HORIZON_RENDERER_ASSET_COOKING)
        if (!ensureSceneGltfCooked(RD_TEXTURES, kSceneSource, RD_OTHER_FILES))
        {
            return false;
        }
#endif
        char catalogFile[FS_MAX_PATH], sceneFile[FS_MAX_PATH];
        fsReplacePathExtension(kSceneSource, "assetcatalog.json", catalogFile);
        fsReplacePathExtension(kSceneSource, "sceneasset.json", sceneFile);
        if (!catalog.load(RD_OTHER_FILES, catalogFile))
        {
            return false;
        }
        const hz::AssetCatalogEntry* entry = catalog.findScene(sceneFile);
        if (!entry)
        {
            LOGF(eERROR, "Catalog contains no SceneAsset '%s'", sceneFile);
            return false;
        }
        pLoaders = hz::make_unique<hz::AssetLoaders>(
            *pContext, hz::AssetLoadersDesc{ .vertexLayout = kSceneVertexLayout, .cookedDirectory = RD_OTHER_FILES });
        pAssetManager = hz::make_unique<hz::AssetManager>(catalog, pContext.get());
        pLoaders->registerLoaders(*pAssetManager);
        const hz::AssetHandle handle = pAssetManager->request(entry->id);
        if (!handle.isValid())
        {
            return false;
        }
        while (pAssetManager->getState(handle) != hz::AssetState::Ready && pAssetManager->getState(handle) != hz::AssetState::Failed)
        {
            pAssetManager->update();
            threadSleep(1);
        }
        const hz::SceneAsset* asset = pAssetManager->getScene(handle);
        if (!asset)
        {
            LOGF(eERROR, "SceneAsset '%s' failed to load (error %u)", sceneFile, (uint32_t)pAssetManager->getError(handle));
            return false;
        }
        pTypes = hz::make_unique<hz::TypeRegistry>();
        const bool registered = pTypes->registerBuiltins();
        ASSERT(registered);
        pWorld = hz::make_unique<hz::World>(*pTypes);
        if (!asset->instantiate(*pWorld).isValid())
        {
            LOGF(eERROR, "Cannot instantiate SceneAsset '%s'", sceneFile);
            return false;
        }
        pScene = hz::make_unique<hz::RenderScene>(*pWorld, *pAssetManager);
        pScene->reserve(asset->getNodes().count, asset->getNodes().count, 1, 1);
        pScene->update();
        LOGF(eINFO, "World ready: %u render primitives, %u instances, %u mesh batches, %u materials, %u textures",
             pScene->getActivePrimitives().count, pScene->getActiveInstances().count, pScene->getActiveMeshBatches().count,
             asset->getMaterials().count, asset->getTextures().count);
        pAssetManager->release(handle);
        return true;
    }

    bool initRenderPasses()
    {
        const RenderPassesDesc desc = {
            .pContext = pContext.get(),
            .pScene = pScene.get(),
            .pAssetManager = pAssetManager.get(),
            .camera = pScene->findCamera(cameraEntity),
        };
        pRenderPasses = hz::make_unique<RenderPasses>(desc);
        ASSERT(pRenderPasses);
        return true;
    }

public:
    void Exit() override
    {
        pCamera = nullptr;
        if (pContext && pContext->isValid())
        {
            pContext->waitIdle();
        }
        pRenderPasses = nullptr;
        pScene = nullptr;
        pWorld = nullptr;
        pTypes = nullptr;
        if (pAssetManager)
        {
            pAssetManager->shutdown();
        }
        pAssetManager = nullptr;
        pLoaders = nullptr;
        catalog.clear();
        cameraEntity = {};
        pContext = nullptr;
    }

    bool Load(ReloadDesc* reload) override
    {
        if (!(reload->type & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET)))
        {
            return true;
        }
        const uint32_t width = (uint32_t)settings.width, height = (uint32_t)settings.height;
        if (!pContext->resize(width, height))
        {
            LOGF(eERROR, "Failed to resize RenderContext to %ux%u", width, height);
            return false;
        }
        return pRenderPasses->resize(width, height);
    }
    void Unload(ReloadDesc*) override { pContext->waitIdle(); }
    void Update(float deltaTime) override
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("Renderer::Update");
        pCamera->update(deltaTime, (uint32_t)settings.width, (uint32_t)settings.height, settings.focused);
        const Matrix4         pose = pCamera->getWorldMatrix();
        const bool            updated = pWorld->setComponent(cameraEntity, hz::SceneTypes::localMatrix, &pose);
        ASSERT(updated);
        pAssetManager->update();
        pRenderPasses->update(deltaTime);
    }
    const char* GetName() override { return "Renderer"; }

    void Draw() override { pRenderPasses->execute(); }

private:
    void initCamera()
    {
        hz::RenderBounds bounds;
        bool             hasBounds = false;
        for (uint32_t index : pScene->getActiveInstances())
        {
            const hz::RenderInstance& instance = pScene->getInstances().pData[index];
            if (!pScene->getPrimitive(instance.primitive)->instanceCount)
            {
                continue;
            }
            if (!hasBounds)
            {
                bounds = instance.bounds;
            }
            else
            {
                for (uint32_t axis = 0; axis < 3; ++axis)
                {
                    bounds.minimum[axis] = fminf(bounds.minimum[axis], instance.bounds.minimum[axis]);
                    bounds.maximum[axis] = fmaxf(bounds.maximum[axis], instance.bounds.maximum[axis]);
                }
            }
            hasBounds = true;
        }
        const float                    cx = (bounds.minimum[0] + bounds.maximum[0]) * 0.5f;
        const float                    cz = (bounds.minimum[2] + bounds.maximum[2]) * 0.5f;
        const FreeCameraControllerDesc desc = {
            .pWindow = pWindow,
            .position = Vector3(cx + 18.0f, bounds.minimum[1] + 16.0f, cz - 32.0f),
            .lookAt = Vector3(cx, bounds.minimum[1] + 4.0f, cz),
            .motion = { .maxSpeed = 8.0f, .acceleration = 40.0f, .braking = 60.0f },
            .boostMultiplier = 4.0f,
        };
        pCamera = hz::make_unique<FreeCameraController>(desc);
        cameraEntity = pWorld->createPersistentEntity();
        const hz::Camera      camera = { .verticalFov = PI / 4.0f };
        const Matrix4         pose = pCamera->getWorldMatrix();
        // Cookbook theta=90, phi=-26; both scene imports preserve the same Y-up basis.
        const float           sunAngle = 26.0f * PI / 180.0f;
        const Vector3         z(-sinf(sunAngle), cosf(sunAngle), 0);
        const Vector3         x = normalize(cross(Vector3(0, 1, 0), z));
        const hz::Light       sun = { .type = hz::LightType::Directional, .color = Vector3(1, 1, 1), .intensity = 1000 };
        const Matrix4         sunPose = Matrix4(Vector4(x, 0), Vector4(cross(z, x), 0), Vector4(z, 0), Vector4(0, 0, 0, 1));
        const hz::Entity      sunEntity = pWorld->createPersistentEntity();
        const bool            created = pWorld->setComponent(cameraEntity, hz::SceneTypes::camera, &camera) &&
                             pWorld->setComponent(cameraEntity, hz::SceneTypes::localMatrix, &pose) &&
                             pWorld->setComponent(sunEntity, hz::SceneTypes::light, &sun) &&
                             pWorld->setComponent(sunEntity, hz::SceneTypes::localMatrix, &sunPose);
        ASSERT(created);
        pScene->update();
        LOGF(eINFO, "Camera: WASD move, Q/E down/up, hold RMB to look, left Shift boost, R reset, Esc release mouse");
    }

    hz::unique_ptr<FreeCameraController> pCamera;
    hz::unique_ptr<hz::RenderContext>    pContext;
    hz::AssetCatalog                     catalog;
    hz::unique_ptr<hz::AssetLoaders>     pLoaders;
    hz::unique_ptr<hz::AssetManager>     pAssetManager;
    hz::unique_ptr<hz::TypeRegistry>     pTypes;
    hz::unique_ptr<hz::World>            pWorld;
    hz::unique_ptr<hz::RenderScene>      pScene;
    hz::unique_ptr<RenderPasses>         pRenderPasses;
    hz::Entity                           cameraEntity;
};
