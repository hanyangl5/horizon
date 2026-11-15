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
#include "Core/ITime.h"
#include "Scene/AssetLoaders.h"
#include "Scene/SceneAsset.h"
#if defined(HORIZON_RENDERER_ASSET_COOKING)
#include "AssetPipeline/IAssetPipeline.h"
#endif

#include "RenderPasses.h"

constexpr const char* SceneDirectory = "Assets/Bistro";
constexpr const char* SceneSource = "BistroExterior.gltf";
constexpr bool        EnableHDR = true;
// GPU-based validation slows GPU work several times over, so only Debug builds enable it by default.
#if defined(_DEBUG)
constexpr bool EnableGpuValidation = true;
#else
constexpr bool EnableGpuValidation = false;
#endif

const VertexLayout SceneVertexLayout = {
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
        ConfigureResourceDirectories();
        BeginSceneCook();
        int64_t Start = GetUSec(true);
        InitRenderContext();
        LOGF(Info, "Startup: RenderContext %.1f ms", (GetUSec(true) - Start) / 1000.0);
        if (!FinishSceneCook())
        {
            Exit();
            return false;
        }
        Start = GetUSec(true);
        if (!InitSceneAsset())
        {
            Exit();
            return false;
        }
        LOGF(Info, "Startup: scene load/instantiate %.1f ms", (GetUSec(true) - Start) / 1000.0);
        InitCamera();
        Start = GetUSec(true);
        const bool Ready = InitRenderPasses();
        LOGF(Info, "Startup: render pass shaders and pipelines %.1f ms", (GetUSec(true) - Start) / 1000.0);
        if (Ready)
        {
            // Persist the driver PSOs built here so the next start only validates them.
            Context->savePipelineCache();
        }
        return Ready;
    }

private:
    void ConfigureResourceDirectories()
    {
        SetResourceDirectoryPath(SystemFileIO, RM_SAVE_0, RD_MESHES, "RendererAssets");
        SetResourceDirectoryPath(SystemFileIO, RM_SAVE_0, RD_OTHER_FILES, "RendererAssets");
        SetResourceDirectoryPath(SystemFileIO, RM_PROJECT, RD_TEXTURES, SceneDirectory);
        SetResourceDirectoryPath(SystemFileIO, RM_PROJECT, RD_SHADER_SOURCES, "Examples/Renderer/Shaders");
        // Compiled DXIL survives restarts here so a warm start skips the Slang front end.
        SetResourceDirectoryPath(SystemFileIO, RM_SAVE_0, RD_SHADER_BINARIES, "ShaderCache");
        MakeDirectory(RD_SHADER_BINARIES, "", true);
    }

    void InitRenderContext()
    {
        const hz::ContextDesc Desc = {
            .pAppName = GetName(),
            .windowHandle = pWindow->handle,
            .imageCount = 3,
            .enableHDR = EnableHDR,
            .enableVSync = settings.vSyncEnabled,
            .enableGpuValidation = EnableGpuValidation,
            .enableGpuProfiler = true,
            .enablePipelineCache = true,
        };
        Context = hz::MakeUnique<hz::RenderContext>(Desc);
    }

    bool InitSceneAsset()
    {
        char CatalogFile[FS_MAX_PATH], SceneFile[FS_MAX_PATH];
        ReplacePathExtension(SceneSource, "assetcatalog.json", CatalogFile);
        ReplacePathExtension(SceneSource, "sceneasset.json", SceneFile);
        if (!Catalog.load(RD_OTHER_FILES, CatalogFile))
        {
            return false;
        }
        const hz::AssetCatalogEntry* Entry = Catalog.findScene(SceneFile);
        if (!Entry)
        {
            LOGF(Error, "Catalog contains no SceneAsset '%s'", SceneFile);
            return false;
        }
        Loaders = hz::MakeUnique<hz::AssetLoaders>(
            *Context, hz::AssetLoadersDesc{ .vertexLayout = SceneVertexLayout, .cookedDirectory = RD_OTHER_FILES });
        AssetManager = hz::MakeUnique<hz::AssetManager>(Catalog, Context.Get());
        Loaders->registerLoaders(*AssetManager);
        const hz::AssetHandle Handle = AssetManager->request(Entry->id);
        if (!Handle.isValid())
        {
            return false;
        }
        while (AssetManager->getState(Handle) != hz::AssetState::Ready && AssetManager->getState(Handle) != hz::AssetState::Failed)
        {
            AssetManager->update();
            ThreadSleep(1);
        }
        const hz::SceneAsset* Asset = AssetManager->getScene(Handle);
        if (!Asset)
        {
            LOGF(Error, "SceneAsset '%s' failed to load (error %u)", SceneFile, (uint32_t)AssetManager->getError(Handle));
            return false;
        }
        Types = hz::MakeUnique<hz::TypeRegistry>();
        const bool Registered = Types->registerBuiltins();
        ASSERT(Registered);
        World = hz::MakeUnique<hz::World>(*Types);
        if (!Asset->instantiate(*World).isValid())
        {
            LOGF(Error, "Cannot instantiate SceneAsset '%s'", SceneFile);
            return false;
        }
        Scene = hz::MakeUnique<hz::RenderScene>(*World, *AssetManager);
        Scene->Reserve(Asset->getNodes().count, Asset->getNodes().count, 1, 1);
        Scene->update();
        LOGF(Info, "World ready: %u render primitives, %u instances, %u mesh batches, %u materials, %u textures",
             Scene->getActivePrimitives().count, Scene->getActiveInstances().count, Scene->getActiveMeshBatches().count,
             Asset->getMaterials().count, Asset->getTextures().count);
        AssetManager->release(Handle);
        return true;
    }

    bool InitRenderPasses()
    {
        const RenderPassesDesc Desc = {
            .Context = Context.Get(),
            .Scene = Scene.Get(),
            .AssetManager = AssetManager.Get(),
            .Camera = Scene->findCamera(CameraEntity),
        };
        Passes = hz::MakeUnique<RenderPasses>(Desc);
        ASSERT(Passes);
        return true;
    }

public:
    void Exit() override
    {
        Camera = nullptr;
        if (Context && Context->isValid())
        {
            Context->waitIdle();
        }
        Passes = nullptr;
        Scene = nullptr;
        World = nullptr;
        Types = nullptr;
        if (AssetManager)
        {
            AssetManager->shutdown();
        }
        AssetManager = nullptr;
        Loaders = nullptr;
        Catalog.Clear();
        CameraEntity = {};
        Context = nullptr;
    }

    bool Load(ReloadDesc* Reload) override
    {
        if (!(Reload->type & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET)))
        {
            return true;
        }
        const uint32_t Width = (uint32_t)settings.width, Height = (uint32_t)settings.height;
        if (!Context->resize(Width, Height))
        {
            LOGF(Error, "Failed to resize RenderContext to %ux%u", Width, Height);
            return false;
        }
        const int64_t Start = GetUSec(true);
        const bool    Ready = Passes->Resize(Width, Height);
        LOGF(Info, "Startup/resize: targets and environment %.1f ms", (GetUSec(true) - Start) / 1000.0);
        return Ready;
    }
    void Unload(ReloadDesc*) override { Context->waitIdle(); }
    void Update(float DeltaTime) override
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("Renderer::Update");
        Camera->update(DeltaTime, (uint32_t)settings.width, (uint32_t)settings.height, settings.focused);
        const Matrix4 Pose = Camera->getWorldMatrix();
        const bool    Updated = World->setComponent(CameraEntity, hz::SceneTypes::localMatrix, &Pose);
        ASSERT(Updated);
        AssetManager->update();
        Passes->Update(DeltaTime);
    }
    const char* GetName() override { return "Renderer"; }

    void Draw() override { Passes->Execute(); }

private:
    // The glTF cook check only touches files, so it runs while the device is created.
    void BeginSceneCook()
    {
        ThreadDesc Thread = { .pFunc = SceneCookTask, .pData = this, .threadName = "SceneCook" };
        if (!InitThread(&Thread, &CookThread))
            CookThread = nullptr;
    }

    bool FinishSceneCook()
    {
        if (CookThread)
        {
            JoinThread(CookThread);
            CookThread = nullptr;
        }
        else
        {
            SceneCookTask(this);
        }
        if (!CookSucceeded)
            LOGF(Error, "Failed to cook scene '%s'", SceneSource);
        return CookSucceeded;
    }

    static void SceneCookTask(void* Data)
    {
        RendererApp* App = (RendererApp*)Data;
        // Only builds that link the AssetPipeline tool can cook; shipping builds receive cooked assets instead.
#if defined(HORIZON_RENDERER_ASSET_COOKING)
        App->CookSucceeded = ensureSceneGltfCooked(RD_TEXTURES, SceneSource, RD_OTHER_FILES);
#else
        App->CookSucceeded = true;
#endif
    }

    void InitCamera()
    {
        hz::RenderBounds Bounds;
        bool             HasBounds = false;
        for (uint32_t Index : Scene->getActiveInstances())
        {
            const hz::RenderInstance& Instance = Scene->getInstances().pData[Index];
            if (!Scene->getPrimitive(Instance.primitive)->instanceCount)
            {
                continue;
            }
            if (!HasBounds)
            {
                Bounds = Instance.bounds;
            }
            else
            {
                for (uint32_t Axis = 0; Axis < 3; ++Axis)
                {
                    Bounds.minimum[Axis] = fminf(Bounds.minimum[Axis], Instance.bounds.minimum[Axis]);
                    Bounds.maximum[Axis] = fmaxf(Bounds.maximum[Axis], Instance.bounds.maximum[Axis]);
                }
            }
            HasBounds = true;
        }
        const float                    CenterX = (Bounds.minimum[0] + Bounds.maximum[0]) * 0.5f;
        const float                    CenterZ = (Bounds.minimum[2] + Bounds.maximum[2]) * 0.5f;
        const FreeCameraControllerDesc Desc = {
            .pWindow = pWindow,
            .position = Vector3(-25, 8, -9),
            .lookAt = Vector3(CenterX, Bounds.minimum[1] + 4.0f, CenterZ),
            .motion = { .maxSpeed = 8.0f, .acceleration = 40.0f, .braking = 60.0f },
            .boostMultiplier = 4.0f,
        };
        Camera = hz::MakeUnique<FreeCameraController>(Desc);
        CameraEntity = World->createPersistentEntity();
        const hz::Camera      CameraData = { .verticalFov = PI / 3.0f };
        const Matrix4         Pose = Camera->getWorldMatrix();
        // Cookbook theta=90, phi=-26; both scene imports preserve the same Y-up basis.
        const float           SunAngle = 26.0f * PI / 180.0f;
        const Vector3         Z(-sinf(SunAngle), cosf(SunAngle), 0);
        const Vector3         X = normalize(cross(Vector3(0, 1, 0), Z));
        const hz::Light       Sun = { .type = hz::LightType::Directional, .color = Vector3(1, 1, 1), .intensity = 1000 };
        const Matrix4         SunPose = Matrix4(Vector4(X, 0), Vector4(cross(Z, X), 0), Vector4(Z, 0), Vector4(0, 0, 0, 1));
        const hz::Entity      SunEntity = World->createPersistentEntity();
        const bool            Created = World->setComponent(CameraEntity, hz::SceneTypes::camera, &CameraData) &&
                             World->setComponent(CameraEntity, hz::SceneTypes::localMatrix, &Pose) &&
                             World->setComponent(SunEntity, hz::SceneTypes::light, &Sun) &&
                             World->setComponent(SunEntity, hz::SceneTypes::localMatrix, &SunPose);
        ASSERT(Created);
        Scene->update();
        LOGF(Info, "Camera: WASD move, Q/E down/up, hold RMB to look, left Shift boost, R reset, Esc release mouse");
    }

    hz::UniquePtr<FreeCameraController> Camera;
    hz::UniquePtr<hz::RenderContext>    Context;
    hz::AssetCatalog                     Catalog;
    hz::UniquePtr<hz::AssetLoaders>     Loaders;
    hz::UniquePtr<hz::AssetManager>     AssetManager;
    hz::UniquePtr<hz::TypeRegistry>     Types;
    hz::UniquePtr<hz::World>            World;
    hz::UniquePtr<hz::RenderScene>      Scene;
    hz::UniquePtr<RenderPasses>         Passes;
    hz::Entity                           CameraEntity;
    ThreadHandle                         CookThread = nullptr;
    bool                                 CookSucceeded = false;
};
