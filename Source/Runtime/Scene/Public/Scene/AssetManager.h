/* Copyright (c) 2026 Horizon */
#pragma once

#include "Scene/AssetCatalog.h"
#include "Scene/GpuAssetTables.h"

namespace hz
{
struct AssetHandle
{
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;

    bool isValid() const { return index != UINT32_MAX && generation != 0; }
    bool operator==(const AssetHandle&) const = default;
};

enum class AssetState : uint32_t
{
    Invalid,
    Queued,
    LoadingCPU,
    WaitingDependencies,
    UploadingGPU,
    Ready,
    Failed,
    Retiring,
};

enum class AssetError : uint32_t
{
    None,
    UnknownAsset,
    MissingLoader,
    DependencyCycle,
    DependencyFailed,
    LoadFailed,
    UploadFailed,
    Retiring,
    ShuttingDown,
};

enum class AssetLoadStatus : uint32_t
{
    Pending,
    Succeeded,
    Failed,
};

struct AssetLoadResult
{
    AssetLoadStatus status = AssetLoadStatus::Pending;
    void*           pPayload = nullptr;
    // Optional human readable reason for a failure; the asset manager copies it during poll.
    const char* pError = nullptr;
};

class AssetManager;

// Loader callbacks run on the asset manager's owning thread. begin/poll/upload must not wait for IO or GPU work.
// A terminal poll transfers its payload to the asset manager, including partial failure results.
// Callbacks may query dependencies, but must not mutate the asset manager. Payload Type matches the registered AssetType.
struct AssetLoader
{
    void* UserData = nullptr;
    void* (*pBegin)(const AssetCatalogEntry&, AssetHandle, void*) = nullptr;
    AssetLoadResult (*pPoll)(void* pRequest, void* UserData) = nullptr;
    // On shutdown this must drain unfinished work before destroying the request.
    void (*pDestroyRequest)(void* pRequest, void* UserData) = nullptr;
    void (*pDestroyPayload)(void* pPayload, void* UserData) = nullptr;
    // Optional GPU phase, called after all dependencies are ready. pollUpload owns failure detection and drains partial uploads.
    void (*pUpload)(void* pPayload, const AssetManager&, Span<AssetHandle> dependencies, void* UserData) = nullptr;
    AssetLoadStatus (*pPollUpload)(void* pPayload, void* UserData) = nullptr;
    void (*pWaitUpload)(void* pPayload, void* UserData) = nullptr;
};

class SceneAsset;
class TextureAsset;
class MeshAsset;
class MaterialAsset;
class AssetManagerData;
class RenderContext;

// Single-threaded mutation. Catalog entries, loader contexts and the render context must outlive the asset manager.
class AssetManager
{
public:
    // With a render context, released payloads are destroyed only after GPU work that could reference them completes.
    explicit AssetManager(const AssetCatalog& catalog, RenderContext* pContext = nullptr);
    ~AssetManager();
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    void                 setLoader(AssetType Type, const AssetLoader& loader);
    AssetHandle          request(AssetID id, AssetError* pError = nullptr);
    bool                 release(AssetHandle Handle);
    void                 update();
    AssetState           getState(AssetHandle Handle) const;
    AssetError           getError(AssetHandle Handle) const;
    // Empty until the asset fails; the loader reason when it provided one, otherwise the asset path.
    const char*          getDiagnostic(AssetHandle Handle) const;
    const SceneAsset*    getScene(AssetHandle Handle) const;
    const TextureAsset*  getTexture(AssetHandle Handle) const;
    const MeshAsset*     getMesh(AssetHandle Handle) const;
    const MaterialAsset* getMaterial(AssetHandle Handle) const;
    // Lookup does not acquire a reference; use request() to retain the asset.
    AssetHandle          find(AssetID id) const;
    // Encodes the ready payload set once per revision. Views expire when the ready set changes.
    GpuAssetTables       getGpuTables();
    uint32_t             getCount() const;
    void                 shutdown();

private:
    AssetManagerData* Data = nullptr;
};
} // namespace hz
