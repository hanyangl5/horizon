#pragma once
#include "Application/IApp.h"
#include "Core/ICVar.h"
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
#include "Scene/AnimationSystem.h"
#include "Scene/SceneAsset.h"
#if defined(HORIZON_RENDERER_ASSET_COOKING)
#include "AssetPipeline/IAssetPipeline.h"
#endif

#include "RenderPasses.h"
#if defined(HORIZON_EDITOR_MODE)
#include "RendererConstants.h"
#include "Editor/SceneInspector.h"
#endif

inline hz::CVar GVSync("r.VSync", true, "Synchronize presentation to the display.");
inline hz::CVar GFreezeLights("bistro.FreezeLights", false, "Pause moving point lights and their periodic replacement.");

constexpr const char* SceneDirectory = "Assets/Bistro";
constexpr const char* SceneSource = "BistroExterior.gltf";
// A skinned character instantiated into the scene at a street-level spot; its first animation plays from startup.
constexpr const char* CharacterDirectory = "Assets/BrainStem/glTF";
constexpr const char* CharacterSource = "BrainStem.gltf";
constexpr float       CharacterPosition[3] = { -15.0f, 0.4f, -4.6f };
constexpr float       CharacterYaw = 0.0f;
constexpr uint32_t    BistroPointLightCount = 2027;
constexpr float       BistroLightGridWidth = 50.0f; // Horizontal coverage in meters.
constexpr float       BistroLightHeight = 4.0f;
constexpr bool        EnableHDR = true;
// GPU-based validation slows GPU work several times over, so only Debug builds enable it by default.
#if defined(_DEBUG)
constexpr bool EnableGpuValidation = true;
#else
constexpr bool EnableGpuValidation = false;
#endif

// Texture coordinates, joints and weights are optional: a mesh without them has no stream at that binding.
const VertexLayout SceneVertexLayout = {
    .bindings = { { .stride = 12 }, { .stride = 4 }, { .stride = 4 }, { .stride = 8 }, { .stride = 16 } },
    .attribs = {
        { .semantic = SEMANTIC_POSITION, .Format = hz::Format::R32G32B32_SFLOAT, .binding = 0, .location = 0 },
        { .semantic = SEMANTIC_NORMAL, .Format = hz::Format::R32_UINT, .binding = 1, .location = 1 },
        { .semantic = SEMANTIC_TEXCOORD0, .Format = hz::Format::R32_UINT, .binding = 2, .location = 2 },
        { .semantic = SEMANTIC_JOINTS, .Format = hz::Format::R16G16B16A16_UINT, .binding = 3, .location = 3 },
        { .semantic = SEMANTIC_WEIGHTS, .Format = hz::Format::R32G32B32A32_SFLOAT, .binding = 4, .location = 4 },
    },
    .bindingCount = 5, .attribCount = 5,
};

class RendererApp final: public IApp
{
public:
    RendererApp()
    {
        settings.Width = 3840;
        settings.Height = 2160;
        settings.vSyncEnabled = GVSync.GetBool();
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
        AddBistroPointLights(Camera->GetPosition());
        Scene->Update();
        LOGF(Info, "Scene lighting: %u active lights, including %u Bistro point lights", Scene->GetActiveLights().Count,
             BistroPointLightCount);
        Start = GetUSec(true);
        const bool Ready = InitRenderPasses();
        LOGF(Info, "Startup: render pass shaders and pipelines %.1f ms", (GetUSec(true) - Start) / 1000.0);
        if (Ready)
        {
            // Persist the driver PSOs built here so the next start only validates them.
            Context->SavePipelineCache();
            GVSync.SetApplyBool(
                [](void* User, bool Enabled)
                {
                    RendererApp& App = *static_cast<RendererApp*>(User);
                    if (!App.Context->SetVSync(Enabled))
                    {
                        return false;
                    }
                    App.settings.vSyncEnabled = Enabled;
                    return true;
                },
                this);
        }
        return Ready;
    }

private:
    void ConfigureResourceDirectories()
    {
        SetResourceDirectoryPath(SystemFileIO, MountSave0, DirectoryMeshes, "RendererAssets");
        SetResourceDirectoryPath(SystemFileIO, MountSave0, DirectoryOtherFiles, "RendererAssets");
        SetResourceDirectoryPath(SystemFileIO, MountProject, DirectoryTextures, SceneDirectory);
        // The character's source directory, read only while cooking.
        SetResourceDirectoryPath(SystemFileIO, MountProject, DirectoryAnimations, CharacterDirectory);
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
            .enableVSync = GVSync.GetBool(),
            .enableGpuValidation = EnableGpuValidation,
            .enableGpuProfiler = true,
            .enablePipelineCache = true,
        };
        Context = hz::MakeUnique<hz::RenderContext>(Desc);
    }

    // Merges a cooked scene's catalog into Catalog and returns the scene's entry.
    const hz::AssetCatalogEntry* LoadSceneCatalog(const char* Source)
    {
        char CatalogFile[FS_MAX_PATH], SceneFile[FS_MAX_PATH];
        ReplacePathExtension(Source, "assetcatalog.json", CatalogFile);
        ReplacePathExtension(Source, "sceneasset.json", SceneFile);
        hz::AssetCatalog Loaded;
        if (!Loaded.Load(DirectoryOtherFiles, CatalogFile) || !Catalog.Merge(Loaded))
        {
            return nullptr;
        }
        const hz::AssetCatalogEntry* Entry = Catalog.FindScene(SceneFile);
        if (!Entry)
        {
            LOGF(Error, "Catalog contains no SceneAsset '%s'", SceneFile);
        }
        return Entry;
    }

    const hz::SceneAsset* WaitForScene(hz::AssetHandle Handle, const char* Source)
    {
        while (AssetManager->GetState(Handle) != hz::AssetState::Ready && AssetManager->GetState(Handle) != hz::AssetState::Failed)
        {
            AssetManager->Update();
            ThreadSleep(1);
        }
        const hz::SceneAsset* Asset = AssetManager->GetScene(Handle);
        if (!Asset)
        {
            LOGF(Error, "SceneAsset '%s' failed to Load (error %u)", Source, (uint32_t)AssetManager->GetError(Handle));
        }
        return Asset;
    }

    bool InitSceneAsset()
    {
        const hz::AssetCatalogEntry* SceneEntry = LoadSceneCatalog(SceneSource);
        const hz::AssetCatalogEntry* CharacterEntry = LoadSceneCatalog(CharacterSource);
        if (!SceneEntry || !CharacterEntry)
        {
            return false;
        }
        Loaders = hz::MakeUnique<hz::AssetLoaders>(
            *Context, hz::AssetLoadersDesc{ .vertexLayout = SceneVertexLayout, .cookedDirectory = DirectoryOtherFiles });
        AssetManager = hz::MakeUnique<hz::AssetManager>(Catalog, Context.Get());
        Loaders->RegisterLoaders(*AssetManager);
        const hz::AssetHandle SceneHandle = AssetManager->Request(SceneEntry->id);
        const hz::AssetHandle CharacterHandle = AssetManager->Request(CharacterEntry->id);
        const hz::SceneAsset* Asset = SceneHandle.IsValid() ? WaitForScene(SceneHandle, SceneSource) : nullptr;
        const hz::SceneAsset* CharacterAsset = CharacterHandle.IsValid() ? WaitForScene(CharacterHandle, CharacterSource) : nullptr;
        if (!Asset || !CharacterAsset)
        {
            return false;
        }
        Types = hz::MakeUnique<hz::TypeRegistry>();
        const bool Registered = Types->RegisterBuiltins();
        ASSERT(Registered);
        World = hz::MakeUnique<hz::World>(*Types);
        const hz::Entity Character = CharacterAsset->Instantiate(*World);
        if (!Asset->Instantiate(*World).IsValid() || !Character.IsValid())
        {
            LOGF(Error, "Cannot instantiate SceneAsset '%s' or '%s'", SceneSource, CharacterSource);
            return false;
        }
        const hz::LocalTransform Placement = {
            .translation = Vector3(CharacterPosition[0], CharacterPosition[1], CharacterPosition[2]),
            .rotation = Quat::rotationY(CharacterYaw),
        };
        const bool Placed = World->SetComponent(Character, hz::SceneTypes::localTransform, &Placement);
        ASSERT(Placed);
        Animation = hz::MakeUnique<hz::AnimationSystem>(*World, *AssetManager);
        Scene = hz::MakeUnique<hz::RenderScene>(*World, *AssetManager, Animation.Get());
        const uint32_t NodeCount = Asset->GetNodes().Count + CharacterAsset->GetNodes().Count;
        Scene->Reserve(NodeCount, NodeCount, BistroPointLightCount + 1, 1);
        Animation->Update(0.0f);
        Scene->Update();
        LOGF(Info, "World ready: %u render primitives, %u instances, %u mesh batches, %u skinned, %u materials, %u textures",
             Scene->GetActivePrimitives().Count, Scene->GetActiveInstances().Count, Scene->GetActiveMeshBatches().Count,
             Scene->GetSkins().Count, Asset->GetMaterials().Count + CharacterAsset->GetMaterials().Count, Asset->GetTextures().Count);
        AssetManager->Release(SceneHandle);
        AssetManager->Release(CharacterHandle);
        return true;
    }

    bool InitRenderPasses()
    {
#if defined(HORIZON_EDITOR_MODE)
        const ViewportGeometry Geometry = { .Shader = "InspectorGeometry.slang",
                                            .DepthFormat = hz::Format::D24_UNORM_S8_UINT,
                                            .BindingSlot = GeometryBindingSlot,
                                            .DrawSlot = GeometryDrawSlot };
        Inspector = hz::MakeUnique<SceneInspector>(pWindow->Handle, *World, *Types, CameraEntity, *Context, Geometry);
#endif
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
        GVSync.SetApplyBool(nullptr, nullptr);
        Camera = nullptr;
        if (Context && Context->IsValid())
        {
            Context->WaitIdle();
        }
        Passes = nullptr;
#if defined(HORIZON_EDITOR_MODE)
        Inspector = nullptr;
#endif
        BistroLights.Reset();
        Scene = nullptr;
        Animation = nullptr;
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
#if defined(HORIZON_EDITOR_MODE)
        Inspector->Picking.Cancel();
#endif
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
        if (!GFreezeLights.GetBool())
        {
            UpdateBistroPointLights(DeltaTime);
        }
        Animation->Update(DeltaTime);
#if defined(HORIZON_EDITOR_MODE)
        Inspector->Update(*Scene);
        if (Inspector->EditedPose == CameraEntity)
        {
            Camera->SetPose(Inspector->NewPose.getTranslation(), -Inspector->NewPose.getCol2().getXYZ());
        }
        else if (Inspector->EditedPose.IsValid())
        {
            for (MovingLight& Motion : BistroLights)
            {
                if (Motion.Entity == Inspector->EditedPose)
                {
                    Motion.Anchor = Inspector->NewPose;
                    Motion.Anchor.setTranslation(Inspector->NewPose.getTranslation() - Motion.Travel * sinf(Motion.Phase));
                    break;
                }
            }
        }
#endif
        Passes->Update(DeltaTime);
    }
    const char* GetName() override { return "Renderer"; }

    void Draw() override
    {
        const RecordedFrame Frame = Passes->Record();
        if (!Frame.Commands)
        {
            return;
        }
#if defined(HORIZON_EDITOR_MODE)
        Inspector->RecordOverlay(*Context, *Frame.Commands, *Frame.Backbuffer, Frame.Constants, Frame.View, Frame.Draws, Frame.Sampler,
                                 Frame.Transparent, Frame.Jitter.X, Frame.Jitter.Y);
#endif
        const hz::SubmitHandle Submission = Context->Submit(*Frame.Commands, Frame.Backbuffer);
#if defined(HORIZON_EDITOR_MODE)
        Inspector->OnPresented(Submission);
#endif
        Passes->EndFrame(Frame.View.FrameIndex, Submission);
    }

private:
    // The cooker runs serially on the main thread, as its interface requires; an unchanged scene only hashes its inputs.
    // Only builds that link the AssetPipeline tool can cook; shipping builds receive cooked assets instead.
    static bool CookScene()
    {
#if defined(HORIZON_RENDERER_ASSET_COOKING)
        if (!ensureSceneGltfCooked(DirectoryTextures, SceneSource, DirectoryOtherFiles) ||
            !ensureSceneGltfCooked(DirectoryAnimations, CharacterSource, DirectoryOtherFiles))
        {
            LOGF(Error, "Failed to cook scene '%s' or '%s'", SceneSource, CharacterSource);
            return false;
        }
#endif
        return true;
    }

    void InitCamera()
    {
        // Frames the character from eye height, a few meters away.
        const Vector3                  Character(CharacterPosition[0], CharacterPosition[1], CharacterPosition[2]);
        const FreeCameraControllerDesc Desc = {
            .pWindow = pWindow,
            .position = Character + Vector3(-3.5f, 1.7f, 2.5f),
            .LookAt = Character + Vector3(0.0f, 0.9f, 0.0f),
            .motion = { .maxSpeed = 8.0f, .acceleration = 40.0f, .braking = 60.0f },
            .boostMultiplier = 4.0f,
            .RequireCaptureToMove = true,
        };
        Camera = hz::MakeUnique<FreeCameraController>(Desc);
        CameraEntity = World->CreatePersistentEntity();
        const hz::Camera CameraData = { .verticalFov = PI / 3.0f };
        const Matrix4    Pose = Camera->GetWorldMatrix();
        // Cookbook theta=90, phi=-26; both scene imports preserve the same Y-up basis.
        const float      SunAngle = 26.0f * PI / 180.0f;
        const Vector3    Z(-sinf(SunAngle), cosf(SunAngle), 0);
        const Vector3    X = normalize(cross(Vector3(0, 1, 0), Z));
        hz::DirectionalLightComponent Sun;
        Sun.Common.Base.Intensity = 100'000.0f;
        const Matrix4    SunPose = Matrix4(Vector4(X, 0), Vector4(cross(Z, X), 0), Vector4(Z, 0), Vector4(0, 0, 0, 1));
        const hz::Entity SunEntity = World->CreatePersistentEntity();
        const bool       Created = World->SetComponent(CameraEntity, hz::SceneTypes::camera, &CameraData) &&
                                   World->SetComponent(CameraEntity, hz::SceneTypes::localMatrix, &Pose) &&
                                   World->SetComponent(SunEntity, hz::SceneTypes::directionalLight, &Sun) &&
                                   World->SetComponent(SunEntity, hz::SceneTypes::localMatrix, &SunPose);
        ASSERT(Created);
        LOGF(Info, "Camera: WASD move, Q/E down/up, hold RMB to look, left Shift boost, R Reset, Esc Release mouse");
    }

    struct MovingLight
    {
        hz::Entity Entity;
        Matrix4    Anchor;
        Vector3    Travel;
        float      Phase;
        float      Speed;
    };

    float RandomLightValue()
    {
        LightSeed = 1664525u * LightSeed + 1013904223u;
        return float(LightSeed >> 8) / 16777216.0f;
    }

    void SpawnBistroLight(MovingLight& Motion)
    {
        const Vector3 HalfSize(BistroLightGridWidth * 0.5f, BistroLightHeight * 0.5f, BistroLightGridWidth * 0.5f);
        const Vector3 Amplitude = minPerElem(HalfSize * 0.1f, Vector3(0.5f));
        Vector3       Anchor;
        for (uint32_t Axis = 0; Axis < 3; ++Axis)
        {
            Anchor[Axis] = BistroLightCenter[Axis] + (2.0f * RandomLightValue() - 1.0f) * (HalfSize[Axis] - Amplitude[Axis]);
            Motion.Travel[Axis] = (2.0f * RandomLightValue() - 1.0f) * Amplitude[Axis];
        }
        Motion.Anchor = Matrix4::translation(Anchor);
        Motion.Phase = RandomLightValue() * (2.0f * PI);
        Motion.Speed = 0.5f + RandomLightValue();
        const float     Red = 0.1f + 0.9f * RandomLightValue();
        const float     Green = 0.1f + 0.9f * RandomLightValue();
        const float     Blue = 0.1f + 0.9f * RandomLightValue();
        hz::PointLightComponent Light;
        Light.Local.Common.Base.Intensity = 50.0f + 150.0f * RandomLightValue();
        Light.Local.Common.Base.LightColor = Vector3(Red, Green, Blue);
        Light.Local.IntensityUnits = hz::LightUnits::Candelas;
        Light.Local.AttenuationRadius = 120.0f;
        Motion.Entity = World->CreatePersistentEntity();
        const bool Created = World->SetComponent(Motion.Entity, hz::SceneTypes::pointLight, &Light);
        ASSERT(Created);
    }

    void AddBistroPointLights(const Vector3& Center)
    {
        BistroLightCenter = Center;
        LightSeed = uint32_t(GetUSec(true));
        ReplacementSeconds = 0;
        BistroLights.Resize(BistroPointLightCount);
        for (MovingLight& Motion : BistroLights)
        {
            SpawnBistroLight(Motion);
        }
        UpdateBistroPointLights(0);
    }

    void UpdateBistroPointLights(float DeltaTime)
    {
        if (BistroLights.Empty())
        {
            return;
        }
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("Bistro point lights");
        ReplacementSeconds += DeltaTime;
        while (ReplacementSeconds >= 1.0)
        {
            ReplacementSeconds -= 1.0;
            MovingLight& Motion = BistroLights[uint32_t(double(RandomLightValue()) * BistroLights.Size())];
            const bool   Removed = World->DestroyEntity(Motion.Entity);
            ASSERT(Removed);
            SpawnBistroLight(Motion);
        }
        for (MovingLight& Motion : BistroLights)
        {
            Motion.Phase += DeltaTime * Motion.Speed;
            if (Motion.Phase >= 2.0f * PI)
            {
                Motion.Phase = fmodf(Motion.Phase, 2.0f * PI);
            }
            Matrix4 Pose = Motion.Anchor;
            Pose.setTranslation(Motion.Anchor.getTranslation() + Motion.Travel * sinf(Motion.Phase));
            const bool Updated = World->SetComponent(Motion.Entity, hz::SceneTypes::localMatrix, &Pose);
            ASSERT(Updated);
        }
    }

    hz::Array<MovingLight> BistroLights;
    Vector3                BistroLightCenter;
    uint32_t               LightSeed = 0;
    double                 ReplacementSeconds = 0;

    hz::UniquePtr<FreeCameraController> Camera;
    hz::UniquePtr<hz::RenderContext>    Context;
    hz::AssetCatalog                    Catalog;
    hz::UniquePtr<hz::AssetLoaders>     Loaders;
    hz::UniquePtr<hz::AssetManager>     AssetManager;
    hz::UniquePtr<hz::TypeRegistry>     Types;
    hz::UniquePtr<hz::World>            World;
    hz::UniquePtr<hz::AnimationSystem>  Animation;
    hz::UniquePtr<hz::RenderScene>      Scene;
    hz::UniquePtr<RenderPasses>         Passes;
#if defined(HORIZON_EDITOR_MODE)
    hz::UniquePtr<SceneInspector>       Inspector;
#endif
    hz::Entity                          CameraEntity;
};
