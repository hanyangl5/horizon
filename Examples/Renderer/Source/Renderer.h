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
        { .semantic = SEMANTIC_POSITION, .Format = hz::Format::R32G32B32_SFLOAT, .binding = 0, .location = 0 },
        { .semantic = SEMANTIC_NORMAL, .Format = hz::Format::R32_UINT, .binding = 1, .location = 1 },
        { .semantic = SEMANTIC_TEXCOORD0, .Format = hz::Format::R32_UINT, .binding = 2, .location = 2 },
    },
    .bindingCount = 3, .attribCount = 3,
};

class RendererApp final: public IApp
{
public:
    RendererApp()
    {
        settings.Width = 3840;
        settings.Height = 2160;
        settings.vSyncEnabled = true;
        settings.showPlatformUI = false;
    }

    bool Init() override
    {
        ConfigureResourceDirectories();
        int64_t Start = GetUSec(true);
        if (!CookScene())
        {
            return false;
        }
        LOGF(Info, "Startup: scene cook check %.1f ms", (GetUSec(true) - Start) / 1000.0);
        Start = GetUSec(true);
        InitRenderContext();
        LOGF(Info, "Startup: RenderContext %.1f ms", (GetUSec(true) - Start) / 1000.0);
        Start = GetUSec(true);
        if (!InitSceneAsset())
        {
            Exit();
            return false;
        }
        LOGF(Info, "Startup: scene Load/instantiate %.1f ms", (GetUSec(true) - Start) / 1000.0);
        InitCamera();
        Start = GetUSec(true);
        const bool Ready = InitRenderPasses();
        LOGF(Info, "Startup: render pass shaders and pipelines %.1f ms", (GetUSec(true) - Start) / 1000.0);
        if (Ready)
        {
            // Persist the driver PSOs built here so the next start only validates them.
            Context->SavePipelineCache();
        }
        return Ready;
    }

private:
    void ConfigureResourceDirectories()
    {
        SetResourceDirectoryPath(SystemFileIO, MountSave0, DirectoryMeshes, "RendererAssets");
        SetResourceDirectoryPath(SystemFileIO, MountSave0, DirectoryOtherFiles, "RendererAssets");
        SetResourceDirectoryPath(SystemFileIO, MountProject, DirectoryTextures, SceneDirectory);
        SetResourceDirectoryPath(SystemFileIO, MountProject, DirectoryShaderSources, "Examples/Renderer/Shaders");
        // Compiled DXIL survives restarts here so a warm start skips the Slang front end.
        SetResourceDirectoryPath(SystemFileIO, MountSave0, DirectoryShaderBinaries, "ShaderCache");
        MakeDirectory(DirectoryShaderBinaries, "", true);
    }

    void InitRenderContext()
    {
        const hz::ContextDesc Desc = {
            .pAppName = GetName(),
            .windowHandle = pWindow->Handle,
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
        if (!Catalog.Load(DirectoryOtherFiles, CatalogFile))
        {
            return false;
        }
        const hz::AssetCatalogEntry* Entry = Catalog.FindScene(SceneFile);
        if (!Entry)
        {
            LOGF(Error, "Catalog contains no SceneAsset '%s'", SceneFile);
            return false;
        }
        Loaders = hz::MakeUnique<hz::AssetLoaders>(
            *Context, hz::AssetLoadersDesc{ .vertexLayout = SceneVertexLayout, .cookedDirectory = DirectoryOtherFiles });
        AssetManager = hz::MakeUnique<hz::AssetManager>(Catalog, Context.Get());
        Loaders->RegisterLoaders(*AssetManager);
        const hz::AssetHandle Handle = AssetManager->Request(Entry->id);
        if (!Handle.IsValid())
        {
            return false;
        }
        while (AssetManager->GetState(Handle) != hz::AssetState::Ready && AssetManager->GetState(Handle) != hz::AssetState::Failed)
        {
            AssetManager->Update();
            ThreadSleep(1);
        }
        const hz::SceneAsset* Asset = AssetManager->GetScene(Handle);
        if (!Asset)
        {
            LOGF(Error, "SceneAsset '%s' failed to Load (error %u)", SceneFile, (uint32_t)AssetManager->GetError(Handle));
            return false;
        }
        Types = hz::MakeUnique<hz::TypeRegistry>();
        const bool Registered = Types->RegisterBuiltins();
        ASSERT(Registered);
        World = hz::MakeUnique<hz::World>(*Types);
        if (!Asset->Instantiate(*World).IsValid())
        {
            LOGF(Error, "Cannot instantiate SceneAsset '%s'", SceneFile);
            return false;
        }
        Scene = hz::MakeUnique<hz::RenderScene>(*World, *AssetManager);
        Scene->Reserve(Asset->GetNodes().Count, Asset->GetNodes().Count, 1, 1);
        Scene->Update();
        LOGF(Info, "World ready: %u render primitives, %u instances, %u mesh batches, %u materials, %u textures",
             Scene->GetActivePrimitives().Count, Scene->GetActiveInstances().Count, Scene->GetActiveMeshBatches().Count,
             Asset->GetMaterials().Count, Asset->GetTextures().Count);
        AssetManager->Release(Handle);
        return true;
    }

    bool InitRenderPasses()
    {
        const RenderPassesDesc Desc = {
            .Context = Context.Get(),
            .Scene = Scene.Get(),
            .AssetManager = AssetManager.Get(),
            .Camera = Scene->FindCamera(CameraEntity),
        };
        Passes = hz::MakeUnique<RenderPasses>(Desc);
        ASSERT(Passes);
        return true;
    }

public:
    void Exit() override
    {
        Camera = nullptr;
        if (Context && Context->IsValid())
        {
            Context->WaitIdle();
        }
        Passes = nullptr;
        Scene = nullptr;
        World = nullptr;
        Types = nullptr;
        if (AssetManager)
        {
            AssetManager->Shutdown();
        }
        AssetManager = nullptr;
        Loaders = nullptr;
        Catalog.Clear();
        CameraEntity = {};
        Context = nullptr;
    }

    bool Load(ReloadDesc* Reload) override
    {
        if (!(Reload->Type & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET)))
        {
            return true;
        }
        const uint32_t Width = (uint32_t)settings.Width, Height = (uint32_t)settings.Height;
        if (!Context->Resize(Width, Height))
        {
            LOGF(Error, "Failed to Resize RenderContext to %ux%u", Width, Height);
            return false;
        }
        const int64_t Start = GetUSec(true);
        const bool    Ready = Passes->Resize(Width, Height);
        LOGF(Info, "Startup/Resize: targets and environment %.1f ms", (GetUSec(true) - Start) / 1000.0);
        return Ready;
    }
    void Unload(ReloadDesc*) override { Context->WaitIdle(); }
    void Update(float DeltaTime) override
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("Renderer::Update");
        Camera->Update(DeltaTime, (uint32_t)settings.Width, (uint32_t)settings.Height, settings.focused);
        const Matrix4 Pose = Camera->GetWorldMatrix();
        const bool    Updated = World->SetComponent(CameraEntity, hz::SceneTypes::localMatrix, &Pose);
        ASSERT(Updated);
        AssetManager->Update();
        Passes->Update(DeltaTime);
    }
    const char* GetName() override { return "Renderer"; }

    void Draw() override { Passes->Execute(); }

private:
    // The cooker runs serially on the main thread, as its interface requires; an unchanged scene only hashes its inputs.
    // Only builds that link the AssetPipeline tool can cook; shipping builds receive cooked assets instead.
    static bool CookScene()
    {
#if defined(HORIZON_RENDERER_ASSET_COOKING)
        if (!ensureSceneGltfCooked(DirectoryTextures, SceneSource, DirectoryOtherFiles))
        {
            LOGF(Error, "Failed to cook scene '%s'", SceneSource);
            return false;
        }
#endif
        return true;
    }

    void InitCamera()
    {
        hz::RenderBounds Bounds;
        bool             HasBounds = false;
        for (uint32_t Index : Scene->GetActiveInstances())
        {
            const hz::RenderInstance& Instance = Scene->GetInstances().Data[Index];
            if (!Scene->GetPrimitive(Instance.primitive)->instanceCount)
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
            .LookAt = Vector3(CenterX, Bounds.minimum[1] + 4.0f, CenterZ),
            .motion = { .maxSpeed = 8.0f, .acceleration = 40.0f, .braking = 60.0f },
            .boostMultiplier = 4.0f,
        };
        Camera = hz::MakeUnique<FreeCameraController>(Desc);
        CameraEntity = World->CreatePersistentEntity();
        const hz::Camera CameraData = { .verticalFov = PI / 3.0f };
        const Matrix4    Pose = Camera->GetWorldMatrix();
        // Cookbook theta=90, phi=-26; both scene imports preserve the same Y-up basis.
        const float      SunAngle = 26.0f * PI / 180.0f;
        const Vector3    Z(-sinf(SunAngle), cosf(SunAngle), 0);
        const Vector3    X = normalize(cross(Vector3(0, 1, 0), Z));
        const hz::Light  Sun = { .Type = hz::LightType::Directional, .color = Vector3(1, 1, 1), .intensity = 1000 };
        const Matrix4    SunPose = Matrix4(Vector4(X, 0), Vector4(cross(Z, X), 0), Vector4(Z, 0), Vector4(0, 0, 0, 1));
        const hz::Entity SunEntity = World->CreatePersistentEntity();
        const bool       Created = World->SetComponent(CameraEntity, hz::SceneTypes::camera, &CameraData) &&
                             World->SetComponent(CameraEntity, hz::SceneTypes::localMatrix, &Pose) &&
                             World->SetComponent(SunEntity, hz::SceneTypes::light, &Sun) &&
                             World->SetComponent(SunEntity, hz::SceneTypes::localMatrix, &SunPose);
        ASSERT(Created);
        Scene->Update();
        LOGF(Info, "Camera: WASD move, Q/E down/up, hold RMB to look, left Shift boost, R Reset, Esc Release mouse");
    }

    hz::UniquePtr<FreeCameraController> Camera;
    hz::UniquePtr<hz::RenderContext>    Context;
    hz::AssetCatalog                    Catalog;
    hz::UniquePtr<hz::AssetLoaders>     Loaders;
    hz::UniquePtr<hz::AssetManager>     AssetManager;
    hz::UniquePtr<hz::TypeRegistry>     Types;
    hz::UniquePtr<hz::World>            World;
    hz::UniquePtr<hz::RenderScene>      Scene;
    hz::UniquePtr<RenderPasses>         Passes;
    hz::Entity                          CameraEntity;
};
