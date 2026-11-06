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
};

class AssetManager;

// Loader callbacks run on the asset manager's owning thread. begin/poll/upload must not wait for IO or GPU work.
// A terminal poll transfers its payload to the asset manager, including partial failure results.
// Callbacks may query dependencies, but must not mutate the asset manager. Payload type matches the registered AssetType.
struct AssetLoader
{
    void* pUserData = nullptr;
    void* (*pBegin)(const AssetCatalogEntry&, AssetHandle, void*) = nullptr;
    AssetLoadResult (*pPoll)(void* pRequest, void* pUserData) = nullptr;
    // On shutdown this must drain unfinished work before destroying the request.
    void (*pDestroyRequest)(void* pRequest, void* pUserData) = nullptr;
    void (*pDestroyPayload)(void* pPayload, void* pUserData) = nullptr;
    // Optional GPU phase, called after all dependencies are ready. pollUpload owns failure detection and drains partial uploads.
    void (*pUpload)(void* pPayload, const AssetManager&, Span<AssetHandle> dependencies, void* pUserData) = nullptr;
    AssetLoadStatus (*pPollUpload)(void* pPayload, void* pUserData) = nullptr;
    void (*pWaitUpload)(void* pPayload, void* pUserData) = nullptr;
};

class SceneAsset;
class TextureAsset;
class MeshAsset;
class MaterialAsset;
class AssetManagerData;
class AssetSnapshotData;
class RenderContext;
struct SubmitHandle;

// Pins one immutable set of ready payloads. Destroy unsubmitted snapshots before asset manager shutdown.
class AssetSnapshot
{
public:
    AssetSnapshot() = default;
    ~AssetSnapshot();
    AssetSnapshot(AssetSnapshot&& other) noexcept;
    AssetSnapshot& operator=(AssetSnapshot&& other) noexcept;
    AssetSnapshot(const AssetSnapshot&) = delete;
    AssetSnapshot& operator=(const AssetSnapshot&) = delete;

    bool                  isValid() const { return pData != nullptr; }
    uint64_t              getRevision() const;
    // Available on snapshots returned by captureRenderSnapshot().
    GpuAssetTableSnapshot getGpuTables() const;
    const SceneAsset*     getScene(AssetHandle) const;
    const TextureAsset*   getTexture(AssetHandle) const;
    const MeshAsset*      getMesh(AssetHandle) const;
    const MaterialAsset*  getMaterial(AssetHandle) const;

private:
    AssetSnapshotData* pData = nullptr;
    friend class AssetManager;
};

// Single-threaded mutation. Catalog entries and loader contexts must outlive the asset manager.
class AssetManager
{
public:
    explicit AssetManager(const AssetCatalog& catalog);
    ~AssetManager();
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    void                 setLoader(AssetType type, const AssetLoader& loader);
    AssetHandle          request(AssetID id, AssetError* pError = nullptr);
    bool                 release(AssetHandle handle);
    void                 update();
    AssetState           getState(AssetHandle handle) const;
    AssetError           getError(AssetHandle handle) const;
    const SceneAsset*    getScene(AssetHandle handle) const;
    const TextureAsset*  getTexture(AssetHandle handle) const;
    const MeshAsset*     getMesh(AssetHandle handle) const;
    const MaterialAsset* getMaterial(AssetHandle handle) const;
    // Lookup does not acquire a reference; use request() to retain the asset.
    AssetHandle          find(AssetID id) const;
    AssetSnapshot        captureSnapshot();
    // Encodes GPU indirection tables once per ready payload set. Requires the typed loaders.
    AssetSnapshot        captureRenderSnapshot();
    // Transfers the snapshot lease to the submission fence; context must outlive the asset manager.
    void                 retireSnapshot(AssetSnapshot&& snapshot, RenderContext& context, SubmitHandle submission);
    uint32_t             getCount() const;
    void                 shutdown();

private:
    AssetManagerData* pData = nullptr;
};
} // namespace hz
